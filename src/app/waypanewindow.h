// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "connectionlistmodel.h"

#include <QHash>
#include <QJsonObject>
#include <QQueue>
#include <QUrl>
#include <QWidget>

class KDirOperator;
class QLabel;
class QLineEdit;
class QPushButton;
class QCloseEvent;
class QPlainTextEdit;
class QEvent;
class QResizeEvent;
class QStackedWidget;
class QSplitter;
class QTabWidget;
class QToolButton;
class QTreeView;
class QStandardItemModel;
class ResizeHandle;
class TerminalInterface;

namespace KParts
{
class ReadOnlyPart;
}

class WaypaneWindow : public QWidget
{
    Q_OBJECT

public:
    explicit WaypaneWindow(QWidget *parent = nullptr);
    ~WaypaneWindow() override;

    bool connectToProfileId(const QString &profileId);
    bool openTunnelsProfileId(const QString &profileId);
    bool browseProfileFilesById(const QString &profileId);
    bool editRemoteFileById(const QString &profileId, const QString &remotePath);
    void openLocalTerminal();
    void splitLocalTerminal(Qt::Orientation orientation);
    [[nodiscard]] QJsonObject runtimeStatus() const;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void addConnection();
    void editConnection();
    void deleteConnection();
    void connectSelected();
    void connectSelectedTunnels();
    void browseSelectedFiles();
    void importConnections();
    void uploadFiles();
    void downloadSelected();
    void editRemoteSelected();
    void saveRemoteEdit();
    void closeFilePanel();
    void filterConnections(const QString &text);
    void updateSelection();
    void cycleTheme();
    void openSettings();
    void configurePortForwards();
    void newLocalTerminal();
    void closeTerminal(int index);

private:
    enum class ThemeMode {
        System,
        Light,
        Dark,
    };

    struct TerminalSession {
        KParts::ReadOnlyPart *part = nullptr;
        TerminalInterface *interface = nullptr;
        QString profileId;
    };

    struct TransferRequest {
        QList<QUrl> sources;
        QUrl destination;
        Waypane::ConnectionProfile profile;
        QString sshAlias;
        bool upload = false;
    };

    void buildUi();
    void applyTheme();
    void applyTerminalTheme(TerminalInterface *terminal) const;
    void applyTerminalThemes() const;
    void ensureTerminalThemes() const;
    [[nodiscard]] QString sessionLogPath(const Waypane::ConnectionProfile &profile) const;
    [[nodiscard]] QString helperExecutable(const QString &name) const;
    void refreshConnections(int selectRow = -1);
    [[nodiscard]] std::optional<Waypane::ConnectionProfile> selectedProfile() const;
    [[nodiscard]] TerminalSession createTerminal(const QString &title, QSplitter *workspace = nullptr, const QString &profileId = {});
    [[nodiscard]] bool openSshSession(const Waypane::ConnectionProfile &profile, QSplitter *workspace = nullptr, bool tunnelsOnly = false);
    void restoreWorkspace();
    void saveWorkspace() const;
    void openRemoteEditor(const QUrl &url);
    [[nodiscard]] bool confirmDiscardRemoteEdit();
    [[nodiscard]] bool isDarkTheme() const;
    [[nodiscard]] bool ensureManagedSshConfig();
    void syncManagedSshConfig();
    void enqueueTransfer(const QList<QUrl> &sources, const QUrl &destination, bool upload);
    void startNextTransfer();
    void finishTransfer(const QString &status);
    void copyWithKio(const TransferRequest &request);
    [[nodiscard]] bool startRsyncTransfer(const TransferRequest &request);
    void updateResizeHandles();

    ConnectionListModel m_connections;
    QTreeView *m_connectionList = nullptr;
    QStandardItemModel *m_connectionTreeModel = nullptr;
    QLineEdit *m_search = nullptr;
    QLabel *m_workspaceTitle = nullptr;
    QLabel *m_workspaceEndpoint = nullptr;
    QLabel *m_transferStatus = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_sftpButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QToolButton *m_themeButton = nullptr;
    QStackedWidget *m_terminalStack = nullptr;
    QWidget *m_landingPage = nullptr;
    QTabWidget *m_terminalTabs = nullptr;
    QLabel *m_hostCountLabel = nullptr;
    QSplitter *m_workspaceSplit = nullptr;
    QStackedWidget *m_fileStack = nullptr;
    QWidget *m_fileBrowserPage = nullptr;
    QWidget *m_remoteEditorPage = nullptr;
    KDirOperator *m_localFileBrowser = nullptr;
    KDirOperator *m_fileBrowser = nullptr;
    QLineEdit *m_localPath = nullptr;
    QLineEdit *m_remotePath = nullptr;
    QLabel *m_remoteEditorPath = nullptr;
    QPlainTextEdit *m_remoteEditor = nullptr;
    QUrl m_remoteEditorUrl;
    ThemeMode m_themeMode = ThemeMode::System;
    QHash<QWidget *, TerminalSession> m_terminalSessions;
    QList<ResizeHandle *> m_resizeHandles;
    QString m_activeProfileId;
    QString m_sftpAlias;
    std::optional<Waypane::ConnectionProfile> m_fileProfile;
    QQueue<TransferRequest> m_transferQueue;
    bool m_transferActive = false;
};
