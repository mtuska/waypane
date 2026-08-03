// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "connectionprofile.h"

#include <QList>
#include <QString>

namespace Waypane
{

class ManagedSshConfig
{
public:
    [[nodiscard]] static QString defaultPath();
    [[nodiscard]] static QString userConfigPath();
    [[nodiscard]] static QString aliasFor(const ConnectionProfile &profile);
    [[nodiscard]] static QString render(const QList<ConnectionProfile> &profiles);
    [[nodiscard]] static bool hasInclude(const QString &userConfig = {}, const QString &managedConfig = {});
    [[nodiscard]] static bool isIncluded(const QString &userConfig = {}, const QString &managedConfig = {});
    [[nodiscard]] static bool installInclude(QString *error = nullptr, const QString &userConfig = {}, const QString &managedConfig = {});
    [[nodiscard]] static bool write(const QList<ConnectionProfile> &profiles, QString *error = nullptr, const QString &path = {});
};

}
