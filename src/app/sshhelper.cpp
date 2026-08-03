// SPDX-License-Identifier: GPL-3.0-or-later
#include "secretstore.h"

#include <QApplication>
#include <QFile>
#include <QMessageBox>

#include <cstdio>
#include <unistd.h>
#include <vector>

namespace
{
int askForSecret(const QStringList &arguments)
{
    const QString prompt = arguments.mid(1).join(QLatin1Char(' '));
    if (prompt.contains(QStringLiteral("yes/no"), Qt::CaseInsensitive)
        || prompt.contains(QStringLiteral("authenticity"), Qt::CaseInsensitive)) {
        const auto answer = QMessageBox::question(nullptr,
                                                  QObject::tr("Verify SSH host"),
                                                  prompt,
                                                  QMessageBox::Yes | QMessageBox::No,
                                                  QMessageBox::No);
        std::puts(answer == QMessageBox::Yes ? "yes" : "no");
        return answer == QMessageBox::Yes ? 0 : 1;
    }

    QString error;
    const QString secret = Waypane::SecretStore::read(QString::fromUtf8(qgetenv("WAYPANE_SECRET_ID")), 0, &error);
    if (secret.isEmpty()) {
        if (!error.isEmpty()) {
            std::fprintf(stderr, "waypane-ssh-helper: %s\n", qPrintable(error));
        }
        return 1;
    }
    const QByteArray bytes = secret.toUtf8();
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout);
    std::fputc('\n', stdout);
    return 0;
}
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Tuska"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("tuska.dev"));
    QCoreApplication::setApplicationName(QStringLiteral("waypane-ssh-helper"));

    const QStringList arguments = QCoreApplication::arguments();
    if (qEnvironmentVariableIsSet("WAYPANE_ASKPASS_MODE")) {
        return askForSecret(arguments);
    }
    if (arguments.size() < 6 || arguments.at(1) != QStringLiteral("--ssh-executable") || arguments.at(3) != QStringLiteral("--secret-id")
        || arguments.at(5) != QStringLiteral("--")) {
        std::fprintf(stderr, "usage: waypane-ssh-helper --ssh-executable PATH --secret-id ID -- [ssh arguments...]\n");
        return 2;
    }

    const QByteArray sshExecutable = QFile::encodeName(arguments.at(2));
    qputenv("WAYPANE_SECRET_ID", arguments.at(4).toUtf8());
    qputenv("WAYPANE_ASKPASS_MODE", "1");
    qputenv("SSH_ASKPASS", QFile::encodeName(QCoreApplication::applicationFilePath()));
    qputenv("SSH_ASKPASS_REQUIRE", "force");

    QList<QByteArray> encoded;
    encoded.append(QByteArrayLiteral("ssh"));
    for (const QString &argument : arguments.mid(6)) {
        encoded.append(argument.toUtf8());
    }
    std::vector<char *> raw;
    raw.reserve(static_cast<size_t>(encoded.size() + 1));
    for (QByteArray &argument : encoded) {
        raw.push_back(argument.data());
    }
    raw.push_back(nullptr);
    ::execv(sshExecutable.constData(), raw.data());
    std::perror("waypane-ssh-helper: exec ssh");
    return 127;
}
