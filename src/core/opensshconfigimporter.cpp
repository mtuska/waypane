// SPDX-License-Identifier: GPL-3.0-or-later
#include "opensshconfigimporter.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTextStream>
#include <QUuid>

namespace Waypane
{
namespace
{
const QUuid importNamespace(QStringLiteral("{a553fd0c-70f3-5cbe-a340-231824d91749}"));

QString expandedPath(QString value)
{
    if (value.startsWith(QStringLiteral("~/"))) {
        value.replace(0, 1, QDir::homePath());
    }
    return value;
}

bool enabledValue(const QString &value)
{
    return value.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}
}

QList<ConnectionProfile> OpenSshConfigImporter::importFile(const QString &path, QString *error)
{
    QFile file(expandedPath(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return {};
    }

    QList<ConnectionProfile> result;
    QStringList aliases;
    ConnectionProfile current;

    const auto flush = [&] {
        for (const QString &alias : aliases) {
            if (alias.contains(QLatin1Char('*')) || alias.contains(QLatin1Char('?')) || alias.startsWith(QLatin1Char('!'))) {
                continue;
            }
            ConnectionProfile profile = current;
            profile.id = QUuid::createUuidV5(importNamespace, alias.toUtf8()).toString(QUuid::WithoutBraces);
            profile.name = alias;
            profile.sshConfigAlias = alias;
            if (profile.host.isEmpty()) {
                profile.host = alias;
            }
            profile.group = QStringLiteral("OpenSSH");
            if (profile.isValid()) {
                result.append(profile);
            }
        }
    };

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const qsizetype comment = line.indexOf(QStringLiteral(" #"));
        if (comment >= 0) {
            line.truncate(comment);
        }
        const QStringList parts = QProcess::splitCommand(line);
        if (parts.size() < 2) {
            continue;
        }
        const QString keyword = parts.first().toLower();
        const QString value = parts.mid(1).join(QLatin1Char(' '));

        if (keyword == QStringLiteral("host")) {
            flush();
            aliases = parts.mid(1);
            current = {};
            continue;
        }
        if (aliases.isEmpty() || keyword == QStringLiteral("match")) {
            continue;
        }
        if (keyword == QStringLiteral("hostname")) {
            current.host = value;
        } else if (keyword == QStringLiteral("user")) {
            current.username = value;
        } else if (keyword == QStringLiteral("port")) {
            bool ok = false;
            const uint port = value.toUInt(&ok);
            if (ok && port > 0 && port <= 65535) {
                current.port = static_cast<quint16>(port);
            }
        } else if (keyword == QStringLiteral("identityfile")) {
            current.identityFile = expandedPath(value);
            current.authentication = QStringLiteral("key");
        } else if (keyword == QStringLiteral("proxyjump") && value.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0) {
            current.jumpHosts = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
        } else if (keyword == QStringLiteral("localforward")) {
            current.localForwards.append(value);
        } else if (keyword == QStringLiteral("remoteforward")) {
            current.remoteForwards.append(value);
        } else if (keyword == QStringLiteral("dynamicforward")) {
            current.dynamicForwards.append(value);
        } else if (keyword == QStringLiteral("forwardagent")) {
            current.agentForwarding = enabledValue(value);
        } else if (keyword == QStringLiteral("compression")) {
            current.compression = enabledValue(value);
        } else if (keyword == QStringLiteral("stricthostkeychecking")) {
            current.hostKeyPolicy = value.toLower();
        } else if (keyword == QStringLiteral("userknownhostsfile")) {
            current.knownHostsFile = expandedPath(value);
        }
    }
    flush();
    if (error) {
        error->clear();
    }
    return result;
}

}
