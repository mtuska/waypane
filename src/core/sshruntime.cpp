// SPDX-License-Identifier: GPL-3.0-or-later
#include "sshruntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace Waypane
{
namespace
{
QString privateExecutable()
{
    const QString override = QString::fromUtf8(qgetenv("WAYPANE_SSH_EXECUTABLE"));
    if (!override.isEmpty() && QFileInfo(override).isExecutable()) {
        return override;
    }
    const QString directory = QCoreApplication::applicationDirPath();
    const QStringList candidates{
        directory + QStringLiteral("/openssh/bin/ssh"),
        directory + QStringLiteral("/../libexec/waypane/openssh/bin/ssh"),
        QStringLiteral("/usr/libexec/waypane/openssh/bin/ssh"),
        QStringLiteral("/usr/local/libexec/waypane/openssh/bin/ssh"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate).isExecutable()) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }
    return {};
}
}

QString SshRuntime::executable()
{
    const QString privatePath = privateExecutable();
    return privatePath.isEmpty() ? QStringLiteral("/usr/bin/ssh") : privatePath;
}

QString SshRuntime::tool(const QString &name)
{
    const QString ssh = privateExecutable();
    if (!ssh.isEmpty()) {
        const QString candidate = QFileInfo(ssh).absoluteDir().filePath(name);
        if (QFileInfo(candidate).isExecutable()) {
            return candidate;
        }
    }
    const QString system = QStringLiteral("/usr/bin/") + name;
    return QFileInfo(system).isExecutable() ? system : QString();
}

bool SshRuntime::isPrivateRuntime()
{
    return !privateExecutable().isEmpty();
}

}
