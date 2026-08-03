// SPDX-License-Identifier: GPL-3.0-or-later
#include "secretstore.h"

#include <KWallet>

#include <memory>

namespace Waypane
{
namespace
{
std::unique_ptr<KWallet::Wallet> openWallet(WId windowId, QString *error)
{
    std::unique_ptr<KWallet::Wallet> wallet(KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), windowId, KWallet::Wallet::Synchronous));
    if (!wallet) {
        if (error) {
            *error = QStringLiteral("The desktop keyring could not be opened or access was declined");
        }
        return {};
    }
    const QString folder = QStringLiteral("Waypane");
    if (!wallet->hasFolder(folder) && !wallet->createFolder(folder)) {
        if (error) {
            *error = QStringLiteral("Could not create the Waypane keyring folder");
        }
        return {};
    }
    if (!wallet->setFolder(folder)) {
        if (error) {
            *error = QStringLiteral("Could not open the Waypane keyring folder");
        }
        return {};
    }
    return wallet;
}
}

bool SecretStore::write(const QString &id, const QString &secret, WId windowId, QString *error)
{
    auto wallet = openWallet(windowId, error);
    if (!wallet) {
        return false;
    }
    if (wallet->writePassword(id, secret) != 0) {
        if (error) {
            *error = QStringLiteral("Could not store the secret in the desktop keyring");
        }
        return false;
    }
    return true;
}

QString SecretStore::read(const QString &id, WId windowId, QString *error)
{
    if (id.isEmpty()) {
        return {};
    }
    auto wallet = openWallet(windowId, error);
    if (!wallet) {
        return {};
    }
    QString secret;
    if (wallet->readPassword(id, secret) != 0) {
        if (error) {
            *error = QStringLiteral("The saved secret was not found in the desktop keyring");
        }
        return {};
    }
    return secret;
}

bool SecretStore::remove(const QString &id, WId windowId, QString *error)
{
    if (id.isEmpty()) {
        return true;
    }
    auto wallet = openWallet(windowId, error);
    if (!wallet) {
        return false;
    }
    if (wallet->removeEntry(id) != 0) {
        if (error) {
            *error = QStringLiteral("Could not remove the secret from the desktop keyring");
        }
        return false;
    }
    return true;
}

}
