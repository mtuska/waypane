// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

namespace
{
std::string printableProfile(const char *value)
{
    std::string result = value ? value : "remote host";
    for (char &character : result) {
        if (character == '\r' || character == '\n' || character == '\t') {
            character = ' ';
        }
    }
    return result.empty() ? "remote host" : result;
}

int waitForChild(pid_t child)
{
    int status = 0;
    while (::waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            std::fprintf(stderr, "waypane-ssh-runner: waitpid: %s\n", std::strerror(errno));
            return 1;
        }
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
}
}

int main(int argc, char *argv[])
{
    std::string profile = "remote host";
    bool legacyEnabled = false;
    int separator = -1;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--") == 0) {
            separator = index;
            break;
        }
        if (index + 1 < argc && std::strcmp(argv[index], "--profile") == 0) {
            profile = printableProfile(argv[++index]);
        } else if (index + 1 < argc && std::strcmp(argv[index], "--legacy-enabled") == 0) {
            legacyEnabled = std::strcmp(argv[++index], "true") == 0;
        } else {
            std::fprintf(stderr, "waypane-ssh-runner: invalid arguments\n");
            return 2;
        }
    }
    if (separator < 0 || separator + 1 >= argc) {
        std::fprintf(stderr,
                     "usage: waypane-ssh-runner --profile NAME --legacy-enabled true|false -- PROGRAM [ARGS...]\n");
        return 2;
    }

    const pid_t child = ::fork();
    if (child < 0) {
        std::fprintf(stderr, "waypane-ssh-runner: fork: %s\n", std::strerror(errno));
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
        ::execv(argv[separator + 1], childArguments.data());
        std::fprintf(stderr, "waypane-ssh-runner: exec %s: %s\n", argv[separator + 1], std::strerror(errno));
        _exit(127);
    }

    const int exitCode = waitForChild(child);
    if (exitCode == 0 || exitCode == 128 + SIGHUP || exitCode == 128 + SIGINT || exitCode == 128 + SIGTERM) {
        return exitCode;
    }

    std::fprintf(stderr,
                 "\n------------------------------------------------------------\n"
                 "Waypane could not establish the SSH session for %s (exit %d).\n"
                 "The OpenSSH diagnostic is shown above.\n",
                 profile.c_str(),
                 exitCode);
    if (legacyEnabled) {
        std::fprintf(stderr,
                     "Legacy compatibility is already enabled; review the diagnostic and server configuration.\n");
    } else {
        std::fprintf(stderr,
                     "If OpenSSH reports 'no matching ... found', edit this connection and enable\n"
                     "Legacy server compatibility, then connect again.\n");
    }
    std::fprintf(stderr, "Press Enter to close this failed session.\n");
    std::fflush(stderr);

    if (::isatty(STDIN_FILENO)) {
        char character = 0;
        while (::read(STDIN_FILENO, &character, 1) == 1 && character != '\n' && character != '\r') {
        }
    }
    return exitCode;
}
