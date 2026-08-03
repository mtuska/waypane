// SPDX-License-Identifier: GPL-3.0-or-later
#include "connectionstore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace Waypane
{

ConnectionStore::ConnectionStore(QString path)
    : m_path(path.isEmpty() ? defaultPath() : std::move(path))
{
}

QString ConnectionStore::path() const
{
    return m_path;
}

QList<ConnectionProfile> ConnectionStore::load(QString *error) const
{
    QFile file(m_path);
    if (!file.exists()) {
        return {};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = parseError.errorString();
        }
        return {};
    }

    QList<ConnectionProfile> profiles;
    const QJsonArray items = document.object().value(QStringLiteral("connections")).toArray();
    profiles.reserve(items.size());
    for (const QJsonValue &item : items) {
        if (item.isObject()) {
            profiles.append(ConnectionProfile::fromJson(item.toObject()));
        }
    }
    return profiles;
}

bool ConnectionStore::save(const QList<ConnectionProfile> &profiles, QString *error) const
{
    const QFileInfo fileInfo(m_path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        if (error) {
            *error = QStringLiteral("Could not create configuration directory");
        }
        return false;
    }

    QJsonArray items;
    for (const ConnectionProfile &profile : profiles) {
        items.append(profile.toJson());
    }
    const QJsonObject root{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("connections"), items}};

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    QFile::setPermissions(m_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

QString ConnectionStore::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/waypane/connections.json");
}

}
