// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

namespace
{
volatile std::sig_atomic_t resized = 1;

void handleResize(int)
{
    resized = 1;
}

std::string utcNow()
{
    std::time_t now = std::time(nullptr);
    std::tm value{};
    gmtime_r(&now, &value);
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &value);
    return buffer;
}

bool writeAll(int descriptor, const char *data, size_t size)
{
    while (size > 0) {
        const ssize_t written = ::write(descriptor, data, size);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        data += written;
        size -= static_cast<size_t>(written);
    }
    return true;
}
}

int main(int argc, char *argv[])
{
    std::string logPath;
    std::string host;
    int separator = -1;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--") == 0) {
            separator = index;
            break;
        }
        if (index + 1 < argc && std::strcmp(argv[index], "--log") == 0) {
            logPath = argv[++index];
        } else if (index + 1 < argc && std::strcmp(argv[index], "--host") == 0) {
            host = argv[++index];
        } else {
            std::fprintf(stderr, "waypane-session-logger: invalid arguments\n");
            return 2;
        }
    }
    if (logPath.empty() || separator < 0 || separator + 1 >= argc) {
        std::fprintf(stderr, "usage: waypane-session-logger --log FILE --host HOST -- PROGRAM [ARGS...]\n");
        return 2;
    }

    const int log = ::open(logPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
    if (log < 0) {
        std::fprintf(stderr, "waypane-session-logger: open %s: %s\n", logPath.c_str(), std::strerror(errno));
        return 1;
    }
    const std::string header = "# Waypane SSH session\n# Host: " + host + "\n# Started UTC: " + utcNow() + "\n\n";
    writeAll(log, header.data(), header.size());

    int master = -1;
    winsize size{};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    const pid_t child = forkpty(&master, nullptr, nullptr, &size);
    if (child < 0) {
        std::fprintf(stderr, "waypane-session-logger: forkpty: %s\n", std::strerror(errno));
        ::close(log);
        return 1;
    }
    if (child == 0) {
        std::vector<char *> childArguments;
        for (int index = separator + 2; index < argc; ++index) {
            childArguments.push_back(argv[index]);
        }
        if (childArguments.empty()) {
            childArguments.push_back(argv[separator + 1]);
        }
        childArguments.push_back(nullptr);
        execv(argv[separator + 1], childArguments.data());
        std::fprintf(stderr, "waypane-session-logger: exec %s: %s\n", argv[separator + 1], std::strerror(errno));
        _exit(127);
    }

    std::signal(SIGWINCH, handleResize);
    std::signal(SIGPIPE, SIG_IGN);
    bool inputOpen = true;
    char buffer[16384];
    while (true) {
        if (resized) {
            resized = 0;
            winsize current{};
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &current) == 0) {
                ioctl(master, TIOCSWINSZ, &current);
            }
        }
        pollfd descriptors[2] = {{master, POLLIN, 0}, {STDIN_FILENO, static_cast<short>(inputOpen ? POLLIN : 0), 0}};
        const int ready = poll(descriptors, 2, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (descriptors[0].revents & POLLIN) {
            const ssize_t count = ::read(master, buffer, sizeof(buffer));
            if (count <= 0) {
                break;
            }
            writeAll(STDOUT_FILENO, buffer, static_cast<size_t>(count));
            writeAll(log, buffer, static_cast<size_t>(count));
        }
        if (descriptors[1].revents & POLLIN) {
            const ssize_t count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
            if (count <= 0) {
                inputOpen = false;
            } else if (!writeAll(master, buffer, static_cast<size_t>(count))) {
                break;
            }
        }
        if (descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            break;
        }
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
    const std::string footer = "\n# Ended UTC: " + utcNow() + "\n";
    writeAll(log, footer.data(), footer.size());
    fsync(log);
    ::close(log);
    ::close(master);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
}
