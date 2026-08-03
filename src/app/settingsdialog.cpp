// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingsdialog.h"
#include "waypane/version.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
QString mcpServerCommand()
{
    if (!qEnvironmentVariable("FLATPAK_ID").isEmpty()) {
        return QStringLiteral("flatpak run --command=waypane-mcp dev.tuska.waypane");
    }
    return QStringLiteral("waypane-mcp");
}

QString mcpJsonConfiguration()
{
    if (!qEnvironmentVariable("FLATPAK_ID").isEmpty()) {
        return QStringLiteral(R"({
  "mcpServers": {
    "waypane": {
      "type": "stdio",
      "command": "flatpak",
      "args": ["run", "--command=waypane-mcp", "dev.tuska.waypane"]
    }
  }
})");
    }
    return QStringLiteral(R"({
  "mcpServers": {
    "waypane": {
      "type": "stdio",
      "command": "waypane-mcp",
      "args": []
    }
  }
})");
}

QString defaultLogDirectory()
{
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) {
        documents = QDir::homePath();
    }
    return QDir(documents).filePath(QStringLiteral("Waypane Logs"));
}
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    setMinimumWidth(560);
    setObjectName(QStringLiteral("settingsDialog"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);
    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("dialogFrame"));
    outer->addWidget(frame);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(22, 0, 22, 22);
    layout->setSpacing(18);

    auto *titleRow = new QHBoxLayout;
    auto *titleColumn = new QVBoxLayout;
    titleColumn->setSpacing(1);
    auto *title = new QLabel(tr("Waypane settings"), frame);
    title->setObjectName(QStringLiteral("dialogTitle"));
    auto *version = new QLabel(tr("Version %1").arg(QStringLiteral(WAYPANE_VERSION)), frame);
    version->setObjectName(QStringLiteral("versionLabel"));
    version->setProperty("muted", true);
    auto *close = new QToolButton(frame);
    close->setObjectName(QStringLiteral("windowCloseButton"));
    close->setText(QStringLiteral("×"));
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    titleColumn->addWidget(title);
    titleColumn->addWidget(version);
    titleRow->addLayout(titleColumn);
    titleRow->addStretch();
    titleRow->addWidget(close);
    layout->addLayout(titleRow);

    auto *appearanceLabel = new QLabel(tr("TERMINAL APPEARANCE"), frame);
    appearanceLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(appearanceLabel);
    auto *appearance = new QFormLayout;
    m_terminalTheme = new QComboBox(frame);
    m_terminalTheme->addItem(tr("Follow Waypane light/dark mode"), QStringLiteral("auto"));
    m_terminalTheme->addItem(tr("Waypane Night"), QStringLiteral("night"));
    m_terminalTheme->addItem(tr("Waypane Day"), QStringLiteral("day"));
    const QString terminalTheme = QSettings().value(QStringLiteral("terminal/theme"), QStringLiteral("auto")).toString();
    m_terminalTheme->setCurrentIndex(qMax(0, m_terminalTheme->findData(terminalTheme)));
    appearance->addRow(tr("Color profile"), m_terminalTheme);
    layout->addLayout(appearance);

    auto *auditLabel = new QLabel(tr("SESSION AUDIT LOGS"), frame);
    auditLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(auditLabel);
    auto *audit = new QFormLayout;
    m_sessionLogging = new QCheckBox(tr("Record SSH terminal output"), frame);
    m_sessionLogging->setChecked(QSettings().value(QStringLiteral("logging/enabled"), false).toBool());
    audit->addRow(QString(), m_sessionLogging);
    auto *directoryRow = new QHBoxLayout;
    m_logDirectory = new QLineEdit(frame);
    m_logDirectory->setText(QSettings().value(QStringLiteral("logging/directory"), defaultLogDirectory()).toString());
    auto *browse = new QToolButton(frame);
    browse->setText(QStringLiteral("…"));
    browse->setToolTip(tr("Choose session log folder"));
    connect(browse, &QToolButton::clicked, this, &SettingsDialog::chooseLogDirectory);
    directoryRow->addWidget(m_logDirectory, 1);
    directoryRow->addWidget(browse);
    audit->addRow(tr("Destination"), directoryRow);
    layout->addLayout(audit);
    auto *auditHelp = new QLabel(tr("Logs are stored under a folder named for the host, with UTC timestamps and owner-only permissions. Password input is not recorded."), frame);
    auditHelp->setObjectName(QStringLiteral("mutedLabel"));
    auditHelp->setWordWrap(true);
    layout->addWidget(auditHelp);

    auto *startupLabel = new QLabel(tr("STARTUP"), frame);
    startupLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(startupLabel);
    m_restoreWorkspace = new QCheckBox(tr("Restore terminal workspace on startup"), frame);
    m_restoreWorkspace->setChecked(QSettings().value(QStringLiteral("workspace/restore"), true).toBool());
    layout->addWidget(m_restoreWorkspace);
    m_reconnectSsh = new QCheckBox(tr("Reconnect saved SSH panes on startup"), frame);
    m_reconnectSsh->setChecked(QSettings().value(QStringLiteral("workspace/reconnectSsh"), false).toBool());
    m_reconnectSsh->setToolTip(tr("Disabled by default to avoid unexpected network connections"));
    layout->addWidget(m_reconnectSsh);

    auto *mcpLabel = new QLabel(tr("AI & MCP INTEGRATION"), frame);
    mcpLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(mcpLabel);
    auto *mcpHelp = new QLabel(tr("Waypane exposes a local stdio MCP server. Copy a setup command and run it in a host terminal; live terminal, SFTP, and tunnel actions require Waypane to be open."), frame);
    mcpHelp->setObjectName(QStringLiteral("mutedLabel"));
    mcpHelp->setWordWrap(true);
    layout->addWidget(mcpHelp);

    auto *mcpButtons = new QHBoxLayout;
    auto *copyCodex = new QPushButton(tr("Copy Codex setup"), frame);
    copyCodex->setObjectName(QStringLiteral("copyCodexMcpButton"));
    copyCodex->setToolTip(tr("Copy the command that registers Waypane for this user in Codex"));
    connect(copyCodex, &QPushButton::clicked, this, &SettingsDialog::copyCodexMcpCommand);
    auto *copyClaude = new QPushButton(tr("Copy Claude setup"), frame);
    copyClaude->setObjectName(QStringLiteral("copyClaudeMcpButton"));
    copyClaude->setToolTip(tr("Copy the command that registers Waypane for this user in Claude Code"));
    connect(copyClaude, &QPushButton::clicked, this, &SettingsDialog::copyClaudeMcpCommand);
    auto *copyJson = new QPushButton(tr("Copy .mcp.json"), frame);
    copyJson->setObjectName(QStringLiteral("copyMcpJsonButton"));
    copyJson->setToolTip(tr("Copy a Claude-compatible project configuration"));
    connect(copyJson, &QPushButton::clicked, this, &SettingsDialog::copyMcpJson);
    mcpButtons->addWidget(copyCodex);
    mcpButtons->addWidget(copyClaude);
    mcpButtons->addWidget(copyJson);
    layout->addLayout(mcpButtons);

    m_mcpCopyStatus = new QLabel(frame);
    m_mcpCopyStatus->setObjectName(QStringLiteral("mutedLabel"));
    m_mcpCopyStatus->setAccessibleName(tr("MCP setup copy status"));
    layout->addWidget(m_mcpCopyStatus);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, frame);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void SettingsDialog::chooseLogDirectory()
{
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Choose session log folder"), m_logDirectory->text());
    if (!directory.isEmpty()) {
        m_logDirectory->setText(directory);
    }
}

void SettingsDialog::copyCodexMcpCommand()
{
    copyMcpText(QStringLiteral("codex mcp add waypane -- ") + mcpServerCommand(), tr("Codex setup command copied. Run it in a host terminal."));
}

void SettingsDialog::copyClaudeMcpCommand()
{
    copyMcpText(QStringLiteral("claude mcp add --transport stdio --scope user waypane -- ") + mcpServerCommand(), tr("Claude setup command copied. Run it in a host terminal."));
}

void SettingsDialog::copyMcpJson()
{
    copyMcpText(mcpJsonConfiguration(), tr("Project configuration copied. Save it as .mcp.json in the project root."));
}

void SettingsDialog::copyMcpText(const QString &text, const QString &description)
{
    QApplication::clipboard()->setText(text);
    m_mcpCopyStatus->setText(description);
}

void SettingsDialog::save()
{
    const QString directory = QDir::cleanPath(m_logDirectory->text().trimmed());
    if (m_sessionLogging->isChecked() && (directory.isEmpty() || !QDir().mkpath(directory))) {
        QMessageBox::warning(this, tr("Session logs"), tr("Waypane could not create the selected log folder."));
        return;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("terminal/theme"), m_terminalTheme->currentData().toString());
    settings.setValue(QStringLiteral("logging/enabled"), m_sessionLogging->isChecked());
    settings.setValue(QStringLiteral("logging/directory"), directory);
    settings.setValue(QStringLiteral("workspace/restore"), m_restoreWorkspace->isChecked());
    settings.setValue(QStringLiteral("workspace/reconnectSsh"), m_reconnectSsh->isChecked());
    accept();
}
