// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "connectionprofile.h"

#include <QList>
#include <QString>

namespace Waypane
{

class OpenSshConfigImporter
{
public:
    [[nodiscard]] static QList<ConnectionProfile> importFile(const QString &path, QString *error = nullptr);
};

}
