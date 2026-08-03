// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/connectionstore.h"

#include <QAbstractListModel>

#include <optional>

class ConnectionListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        GroupRole,
        HostRole,
        EndpointRole,
        TagsRole,
    };
    Q_ENUM(Role)

    explicit ConnectionListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QString error() const;

    Q_INVOKABLE bool addConnection(const QString &name, const QString &host, const QString &username, int port = 22);
    Q_INVOKABLE int importOpenSshConfig();
    Q_INVOKABLE QStringList sshArguments(int row) const;

    [[nodiscard]] std::optional<Waypane::ConnectionProfile> profileAt(int row) const;
    [[nodiscard]] QList<Waypane::ConnectionProfile> profiles() const;
    bool upsertConnection(Waypane::ConnectionProfile profile);
    bool removeConnection(int row);

signals:
    void countChanged();
    void errorChanged();

private:
    bool persist();
    void setError(const QString &error);

    Waypane::ConnectionStore m_store;
    QList<Waypane::ConnectionProfile> m_profiles;
    QString m_error;
};
