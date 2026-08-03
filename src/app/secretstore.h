// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QtGui/qwindowdefs.h>

namespace Waypane
{

class SecretStore
{
public:
    [[nodiscard]] static bool write(const QString &id, const QString &secret, WId windowId = 0, QString *error = nullptr);
    [[nodiscard]] static QString read(const QString &id, WId windowId = 0, QString *error = nullptr);
    [[nodiscard]] static bool remove(const QString &id, WId windowId = 0, QString *error = nullptr);
};

}
