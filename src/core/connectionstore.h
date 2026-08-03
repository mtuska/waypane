// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "connectionprofile.h"

#include <QList>
#include <QString>

namespace Waypane
{

class ConnectionStore
{
public:
    explicit ConnectionStore(QString path = {});

    [[nodiscard]] QString path() const;
    [[nodiscard]] QList<ConnectionProfile> load(QString *error = nullptr) const;
    [[nodiscard]] bool save(const QList<ConnectionProfile> &profiles, QString *error = nullptr) const;

    [[nodiscard]] static QString defaultPath();

private:
    QString m_path;
};

}
