// SPDX-License-Identifier: GPL-3.0-or-later
#include "sshcommandbuilder.h"

namespace Waypane
{

QStringList SshCommandBuilder::arguments(const ConnectionProfile &profile, bool tunnelsOnly, const QString &configurationFile)
{
    QStringList result;
    if (!profile.sshConfigAlias.isEmpty() && !tunnelsOnly) {
        if (!configurationFile.isEmpty()) {
            result.append({QStringLiteral("-F"), configurationFile});
        }
        result.append(profile.sshConfigAlias);
        return result;
    }
    if (!configurationFile.isEmpty()) {
        result.append({QStringLiteral("-F"), configurationFile});
    }
    if (profile.port != 22) {
        result.append({QStringLiteral("-p"), QString::number(profile.port)});
    }
    if (!profile.identityFile.isEmpty()) {
        result.append({QStringLiteral("-i"), profile.identityFile});
    }
    if (profile.authentication == QStringLiteral("password")) {
        result.append({QStringLiteral("-o"), QStringLiteral("PreferredAuthentications=password,keyboard-interactive")});
        result.append({QStringLiteral("-o"), QStringLiteral("PubkeyAuthentication=no")});
    }
    if (!profile.jumpHosts.isEmpty()) {
        result.append({QStringLiteral("-J"), profile.jumpHosts.join(QLatin1Char(','))});
    }
    for (const QString &forward : profile.localForwards) {
        result.append({QStringLiteral("-L"), forward});
    }
    for (const QString &forward : profile.remoteForwards) {
        result.append({QStringLiteral("-R"), forward});
    }
    for (const QString &forward : profile.dynamicForwards) {
        result.append({QStringLiteral("-D"), forward});
    }
    if (profile.agentForwarding) {
        result.append(QStringLiteral("-A"));
    }
    if (profile.compression) {
        result.append(QStringLiteral("-C"));
    }
    if (profile.legacyCompatibility) {
        result.append({QStringLiteral("-o"), QStringLiteral("HostKeyAlgorithms=+ssh-rsa")});
        result.append({QStringLiteral("-o"), QStringLiteral("PubkeyAcceptedAlgorithms=+ssh-rsa")});
        result.append({QStringLiteral("-o"), QStringLiteral("KexAlgorithms=+diffie-hellman-group1-sha1,diffie-hellman-group14-sha1")});
        result.append({QStringLiteral("-o"), QStringLiteral("Ciphers=+aes128-cbc,aes192-cbc,aes256-cbc,3des-cbc")});
        result.append({QStringLiteral("-o"), QStringLiteral("MACs=+hmac-sha1,hmac-sha1-96")});
    }
    if (!profile.hostKeyPolicy.isEmpty() && profile.hostKeyPolicy != QStringLiteral("ask")) {
        result.append({QStringLiteral("-o"), QStringLiteral("StrictHostKeyChecking=") + profile.hostKeyPolicy});
    }
    if (!profile.knownHostsFile.isEmpty()) {
        result.append({QStringLiteral("-o"), QStringLiteral("UserKnownHostsFile=") + profile.knownHostsFile});
    }
    if (tunnelsOnly) {
        result.append({QStringLiteral("-N"), QStringLiteral("-T")});
    }

    QString destination = profile.host;
    if (!profile.username.isEmpty()) {
        destination.prepend(profile.username + QLatin1Char('@'));
    }
    result.append(destination);
    return result;
}

}
