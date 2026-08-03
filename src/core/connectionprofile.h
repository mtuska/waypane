// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace Waypane
{

struct ConnectionProfile {
    QString id;
    QString name;
    QString group;
    QString host;
    quint16 port = 22;
    QString username;
    QString sshConfigAlias;
    QString authentication = QStringLiteral("agent");
    QString identityFile;
    QString secretId;
    QStringList jumpHosts;
    QStringList tags;
    QStringList localForwards;
    QStringList remoteForwards;
    QStringList dynamicForwards;
    QString remotePath = QStringLiteral("/");
    QString hostKeyPolicy = QStringLiteral("ask");
    QString knownHostsFile;
    bool agentForwarding = false;
    bool compression = false;
    bool legacyCompatibility = false;

    [[nodiscard]] bool isValid(QString *error = nullptr) const;
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static ConnectionProfile fromJson(const QJsonObject &json);
};

}
