// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "connectionprofile.h"

#include <QStringList>

namespace Waypane
{

class SshCommandBuilder
{
public:
    [[nodiscard]] static QStringList arguments(const ConnectionProfile &profile, bool tunnelsOnly = false);
};

}
