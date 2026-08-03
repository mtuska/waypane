// SPDX-License-Identifier: GPL-3.0-or-later
#include "livecontrolserver.h"

#include "waypanewindow.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

#include <unistd.h>

LiveControlServer::LiveControlServer(WaypaneWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_server(new QLocalServer(this))
{
    connect(m_server, &QLocalServer::newConnection, this, &LiveControlServer::acceptConnection);
}

QString LiveControlServer::socketPath()
{
    const QString override = QString::fromUtf8(qgetenv("WAYPANE_CONTROL_SOCKET"));
    if (!override.isEmpty()) {
        return override;
    }
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        runtime = QDir::tempPath() + QStringLiteral("/waypane-") + QString::number(getuid());
    }
    return runtime + QStringLiteral("/waypane/control.sock");
}

bool LiveControlServer::start(QString *error)
{
    const QString path = socketPath();
    const QString directory = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(directory)) {
        if (error) {
            *error = tr("Could not create the runtime socket directory");
        }
        return false;
    }
    QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    QLocalSocket probe;
    probe.connectToServer(path);
    if (probe.waitForConnected(75)) {
        if (error) {
            *error = tr("Another Waypane instance already owns the control socket");
        }
        return false;
    }
    QLocalServer::removeServer(path);
    if (!m_server->listen(path)) {
        if (error) {
            *error = m_server->errorString();
        }
        return false;
    }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

void LiveControlServer::acceptConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            while (socket->canReadLine()) {
                QJsonParseError parseError;
                const QJsonDocument request = QJsonDocument::fromJson(socket->readLine(), &parseError);
                QJsonObject response;
                if (parseError.error != QJsonParseError::NoError || !request.isObject()) {
                    response = {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Invalid JSON request")}};
                } else {
                    const QJsonObject object = request.object();
                    const QString method = object.value(QStringLiteral("method")).toString();
                    if (method == QStringLiteral("status")) {
                        response = m_window->runtimeStatus();
                        response.insert(QStringLiteral("ok"), true);
                    } else if (method == QStringLiteral("connect")) {
                        const QString id = object.value(QStringLiteral("id")).toString();
                        const bool connected = !id.isEmpty() && m_window->connectToProfileId(id);
                        response = {{QStringLiteral("ok"), connected}};
                        if (!connected) {
                            response.insert(QStringLiteral("error"), QStringLiteral("Unknown connection profile"));
                        }
                    } else if (method == QStringLiteral("openTunnels")) {
                        const QString id = object.value(QStringLiteral("id")).toString();
                        const bool opened = !id.isEmpty() && m_window->openTunnelsProfileId(id);
                        response = {{QStringLiteral("ok"), opened}};
                        if (!opened) {
                            response.insert(QStringLiteral("error"), QStringLiteral("Unknown connection profile"));
                        }
                    } else if (method == QStringLiteral("localTerminal")) {
                        m_window->openLocalTerminal();
                        response = {{QStringLiteral("ok"), true}};
                    } else if (method == QStringLiteral("splitTerminal")) {
                        const QString direction = object.value(QStringLiteral("direction")).toString();
                        if (direction == QStringLiteral("right") || direction == QStringLiteral("down")) {
                            m_window->splitLocalTerminal(direction == QStringLiteral("down") ? Qt::Vertical : Qt::Horizontal);
                            response = {{QStringLiteral("ok"), true}};
                        } else {
                            response = {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Direction must be right or down")}};
                        }
                    } else if (method == QStringLiteral("browseFiles")) {
                        const QString id = object.value(QStringLiteral("id")).toString();
                        const bool opened = !id.isEmpty() && m_window->browseProfileFilesById(id);
                        response = {{QStringLiteral("ok"), opened}};
                        if (!opened) {
                            response.insert(QStringLiteral("error"), QStringLiteral("Unknown connection profile"));
                        }
                    } else if (method == QStringLiteral("editRemoteFile")) {
                        const QString id = object.value(QStringLiteral("id")).toString();
                        const QString path = object.value(QStringLiteral("path")).toString();
                        const bool opened = !id.isEmpty() && !path.isEmpty() && m_window->editRemoteFileById(id, path);
                        response = {{QStringLiteral("ok"), opened}};
                        if (!opened) {
                            response.insert(QStringLiteral("error"), QStringLiteral("Unknown profile or invalid remote path"));
                        }
                    } else {
                        response = {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Unknown live-control method")}};
                    }
                }
                socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
                socket->flush();
            }
        });
    }
}
