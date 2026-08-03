// SPDX-License-Identifier: GPL-3.0-or-later
#include "connectionlistmodel.h"

#include "core/sshcommandbuilder.h"
#include "core/opensshconfigimporter.h"

#include <QDir>
#include <QUuid>

#include <algorithm>

ConnectionListModel::ConnectionListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    QString loadError;
    m_profiles = m_store.load(&loadError);
    setError(loadError);
}

int ConnectionListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_profiles.size();
}

QVariant ConnectionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_profiles.size()) {
        return {};
    }
    const Waypane::ConnectionProfile &profile = m_profiles.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return profile.name;
    case Qt::ToolTipRole:
        return (profile.username.isEmpty() ? QString() : profile.username + QLatin1Char('@')) + profile.host + QLatin1Char(':')
            + QString::number(profile.port);
    case IdRole:
        return profile.id;
    case NameRole:
        return profile.name;
    case GroupRole:
        return profile.group;
    case HostRole:
        return profile.host;
    case EndpointRole:
        return (profile.username.isEmpty() ? QString() : profile.username + QLatin1Char('@')) + profile.host + QLatin1Char(':')
            + QString::number(profile.port);
    case TagsRole:
        return profile.tags;
    default:
        return {};
    }
}

QHash<int, QByteArray> ConnectionListModel::roleNames() const
{
    return {
        {IdRole, "connectionId"},
        {NameRole, "name"},
        {GroupRole, "group"},
        {HostRole, "host"},
        {EndpointRole, "endpoint"},
        {TagsRole, "tags"},
    };
}

QString ConnectionListModel::error() const
{
    return m_error;
}

bool ConnectionListModel::addConnection(const QString &name, const QString &host, const QString &username, int port)
{
    Waypane::ConnectionProfile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.name = name.trimmed();
    profile.host = host.trimmed();
    profile.username = username.trimmed();
    profile.port = port > 0 && port <= 65535 ? static_cast<quint16>(port) : 22;

    return upsertConnection(profile);
}

int ConnectionListModel::importOpenSshConfig()
{
    QString importError;
    const QList<Waypane::ConnectionProfile> imported = Waypane::OpenSshConfigImporter::importFile(QDir::homePath() + QStringLiteral("/.ssh/config"), &importError);
    if (!importError.isEmpty()) {
        setError(importError);
        return 0;
    }

    const QList<Waypane::ConnectionProfile> previous = m_profiles;
    beginResetModel();
    for (const Waypane::ConnectionProfile &profile : imported) {
        const auto existing = std::find_if(m_profiles.begin(), m_profiles.end(), [&profile](const Waypane::ConnectionProfile &candidate) {
            return candidate.id == profile.id;
        });
        if (existing == m_profiles.end()) {
            m_profiles.append(profile);
        } else {
            *existing = profile;
        }
    }
    endResetModel();

    if (!persist()) {
        beginResetModel();
        m_profiles = previous;
        endResetModel();
        return 0;
    }
    emit countChanged();
    return imported.size();
}

QStringList ConnectionListModel::sshArguments(int row) const
{
    if (row < 0 || row >= m_profiles.size()) {
        return {};
    }
    return Waypane::SshCommandBuilder::arguments(m_profiles.at(row));
}

std::optional<Waypane::ConnectionProfile> ConnectionListModel::profileAt(int row) const
{
    if (row < 0 || row >= m_profiles.size()) {
        return std::nullopt;
    }
    return m_profiles.at(row);
}

QList<Waypane::ConnectionProfile> ConnectionListModel::profiles() const
{
    return m_profiles;
}

bool ConnectionListModel::upsertConnection(Waypane::ConnectionProfile profile)
{
    QString validationError;
    if (!profile.isValid(&validationError)) {
        setError(validationError);
        return false;
    }
    if (profile.id.isEmpty()) {
        profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    const auto existing = std::find_if(m_profiles.begin(), m_profiles.end(), [&profile](const Waypane::ConnectionProfile &candidate) {
        return candidate.id == profile.id;
    });
    if (existing == m_profiles.end()) {
        const int row = m_profiles.size();
        beginInsertRows({}, row, row);
        m_profiles.append(profile);
        endInsertRows();
        if (!persist()) {
            beginRemoveRows({}, row, row);
            m_profiles.removeAt(row);
            endRemoveRows();
            return false;
        }
        emit countChanged();
        return true;
    }

    const int row = std::distance(m_profiles.begin(), existing);
    const Waypane::ConnectionProfile previous = *existing;
    *existing = profile;
    if (!persist()) {
        *existing = previous;
        return false;
    }
    emit dataChanged(index(row), index(row));
    return true;
}

bool ConnectionListModel::removeConnection(int row)
{
    if (row < 0 || row >= m_profiles.size()) {
        setError(QStringLiteral("No connection selected"));
        return false;
    }
    const Waypane::ConnectionProfile removed = m_profiles.at(row);
    beginRemoveRows({}, row, row);
    m_profiles.removeAt(row);
    endRemoveRows();
    if (!persist()) {
        beginInsertRows({}, row, row);
        m_profiles.insert(row, removed);
        endInsertRows();
        return false;
    }
    emit countChanged();
    return true;
}

bool ConnectionListModel::persist()
{
    QString saveError;
    if (!m_store.save(m_profiles, &saveError)) {
        setError(saveError);
        return false;
    }
    setError({});
    return true;
}

void ConnectionListModel::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}
