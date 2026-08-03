// SPDX-License-Identifier: GPL-3.0-or-later
#include "connectionprofile.h"

#include <QJsonArray>
#include <QUuid>

#include <algorithm>

namespace Waypane
{
namespace
{
QStringList stringList(const QJsonValue &value)
{
    QStringList result;
    for (const QJsonValue &item : value.toArray()) {
        if (item.isString()) {
            result.append(item.toString());
        }
    }
    return result;
}
}

bool ConnectionProfile::isValid(QString *error) const
{
    const auto containsLineBreak = [](const QString &value) {
        return value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r')) || value.contains(QChar::Null);
    };
    if (name.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("Connection name is required");
        }
        return false;
    }
    if (host.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("Host is required");
        }
        return false;
    }
    if (containsLineBreak(host) || containsLineBreak(username) || containsLineBreak(identityFile) || containsLineBreak(knownHostsFile)
        || std::any_of(jumpHosts.cbegin(), jumpHosts.cend(), containsLineBreak)
        || std::any_of(localForwards.cbegin(), localForwards.cend(), containsLineBreak)
        || std::any_of(remoteForwards.cbegin(), remoteForwards.cend(), containsLineBreak)
        || std::any_of(dynamicForwards.cbegin(), dynamicForwards.cend(), containsLineBreak)) {
        if (error) {
            *error = QStringLiteral("Connection fields cannot contain line breaks");
        }
        return false;
    }
    if (port == 0) {
        if (error) {
            *error = QStringLiteral("Port must be between 1 and 65535");
        }
        return false;
    }
    if (!QStringList({QStringLiteral("agent"), QStringLiteral("key"), QStringLiteral("password")}).contains(authentication)) {
        if (error) {
            *error = QStringLiteral("Unsupported authentication method");
        }
        return false;
    }
    if (!QStringList({QStringLiteral("ask"), QStringLiteral("accept-new"), QStringLiteral("yes")}).contains(hostKeyPolicy)) {
        if (error) {
            *error = QStringLiteral("Unsupported host key policy");
        }
        return false;
    }
    return true;
}

QJsonObject ConnectionProfile::toJson() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("group"), group},
        {QStringLiteral("host"), host},
        {QStringLiteral("port"), port},
        {QStringLiteral("username"), username},
        {QStringLiteral("sshConfigAlias"), sshConfigAlias},
        {QStringLiteral("authentication"), authentication},
        {QStringLiteral("identityFile"), identityFile},
        {QStringLiteral("secretId"), secretId},
        {QStringLiteral("jumpHosts"), QJsonArray::fromStringList(jumpHosts)},
        {QStringLiteral("tags"), QJsonArray::fromStringList(tags)},
        {QStringLiteral("localForwards"), QJsonArray::fromStringList(localForwards)},
        {QStringLiteral("remoteForwards"), QJsonArray::fromStringList(remoteForwards)},
        {QStringLiteral("dynamicForwards"), QJsonArray::fromStringList(dynamicForwards)},
        {QStringLiteral("remotePath"), remotePath},
        {QStringLiteral("hostKeyPolicy"), hostKeyPolicy},
        {QStringLiteral("knownHostsFile"), knownHostsFile},
        {QStringLiteral("agentForwarding"), agentForwarding},
        {QStringLiteral("compression"), compression},
        {QStringLiteral("legacyCompatibility"), legacyCompatibility},
    };
}

ConnectionProfile ConnectionProfile::fromJson(const QJsonObject &json)
{
    ConnectionProfile profile;
    profile.id = json.value(QStringLiteral("id")).toString();
    if (profile.id.isEmpty()) {
        profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    profile.name = json.value(QStringLiteral("name")).toString();
    profile.group = json.value(QStringLiteral("group")).toString();
    profile.host = json.value(QStringLiteral("host")).toString();
    const int port = json.value(QStringLiteral("port")).toInt(22);
    profile.port = port > 0 && port <= 65535 ? static_cast<quint16>(port) : 22;
    profile.username = json.value(QStringLiteral("username")).toString();
    profile.sshConfigAlias = json.value(QStringLiteral("sshConfigAlias")).toString();
    profile.authentication = json.value(QStringLiteral("authentication")).toString(QStringLiteral("agent"));
    profile.identityFile = json.value(QStringLiteral("identityFile")).toString();
    profile.secretId = json.value(QStringLiteral("secretId")).toString();
    profile.jumpHosts = stringList(json.value(QStringLiteral("jumpHosts")));
    profile.tags = stringList(json.value(QStringLiteral("tags")));
    profile.localForwards = stringList(json.value(QStringLiteral("localForwards")));
    profile.remoteForwards = stringList(json.value(QStringLiteral("remoteForwards")));
    profile.dynamicForwards = stringList(json.value(QStringLiteral("dynamicForwards")));
    profile.remotePath = json.value(QStringLiteral("remotePath")).toString(QStringLiteral("/"));
    profile.hostKeyPolicy = json.value(QStringLiteral("hostKeyPolicy")).toString(QStringLiteral("ask"));
    profile.knownHostsFile = json.value(QStringLiteral("knownHostsFile")).toString();
    profile.agentForwarding = json.value(QStringLiteral("agentForwarding")).toBool(false);
    profile.compression = json.value(QStringLiteral("compression")).toBool(false);
    profile.legacyCompatibility = json.value(QStringLiteral("legacyCompatibility")).toBool(false);
    return profile;
}

}
