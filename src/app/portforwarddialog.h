// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/connectionprofile.h"

#include <QDialog>

class QPlainTextEdit;

class PortForwardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PortForwardDialog(const Waypane::ConnectionProfile &profile, QWidget *parent = nullptr);

    [[nodiscard]] Waypane::ConnectionProfile profile() const;
    [[nodiscard]] bool startRequested() const;

private slots:
    void saveOnly();
    void saveAndStart();

private:
    void finish(bool start);
    [[nodiscard]] static QStringList lines(const QPlainTextEdit *editor);

    Waypane::ConnectionProfile m_profile;
    QPlainTextEdit *m_local = nullptr;
    QPlainTextEdit *m_remote = nullptr;
    QPlainTextEdit *m_dynamic = nullptr;
    bool m_startRequested = false;
};
