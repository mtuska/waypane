// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

class QLocalServer;
class WaypaneWindow;

class LiveControlServer : public QObject
{
    Q_OBJECT

public:
    explicit LiveControlServer(WaypaneWindow *window, QObject *parent = nullptr);

    [[nodiscard]] bool start(QString *error = nullptr);
    [[nodiscard]] static QString socketPath();

private:
    void acceptConnection();

    WaypaneWindow *m_window;
    QLocalServer *m_server;
};
