// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Waypane
{

class SshRuntime
{
public:
    [[nodiscard]] static QString executable();
    [[nodiscard]] static QString tool(const QString &name);
    [[nodiscard]] static bool isPrivateRuntime();
};

}
