// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void chooseLogDirectory();
    void save();

private:
    QComboBox *m_terminalTheme = nullptr;
    QCheckBox *m_sessionLogging = nullptr;
    QLineEdit *m_logDirectory = nullptr;
    QCheckBox *m_restoreWorkspace = nullptr;
    QCheckBox *m_reconnectSsh = nullptr;
};
