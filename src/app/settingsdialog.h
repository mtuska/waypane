// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void chooseLogDirectory();
    void copyCodexMcpCommand();
    void copyClaudeMcpCommand();
    void copyMcpJson();
    void save();

private:
    void copyMcpText(const QString &text, const QString &description);

    QComboBox *m_terminalTheme = nullptr;
    QCheckBox *m_sessionLogging = nullptr;
    QLineEdit *m_logDirectory = nullptr;
    QCheckBox *m_restoreWorkspace = nullptr;
    QCheckBox *m_reconnectSsh = nullptr;
    QLabel *m_mcpCopyStatus = nullptr;
};
