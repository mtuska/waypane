// SPDX-License-Identifier: GPL-3.0-or-later
#include "waypanewindow.h"
#include "waypane/version.h"

#include "core/sshcommandbuilder.h"
#include "core/sshruntime.h"
#include "core/managedsshconfig.h"
#include "profiledialog.h"
#include "portforwarddialog.h"
#include "secretstore.h"
#include "settingsdialog.h"

#include <KDirOperator>
#include <KFile>
#include <KFileItem>
#include <KIO/CopyJob>
#include <KIO/StoredTransferJob>
#include <KParts/ReadOnlyPart>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <kde_terminal_interface.h>

#include <QApplication>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QMimeData>
#include <QMenu>
#include <QMouseEvent>
#include <QProcess>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyleHints>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QPointer>
#include <QTimer>
#include <QWindow>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace
{
class TitleBar : public QFrame
{
public:
    explicit TitleBar(QWidget *parent)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("titleBar"));
        setFixedHeight(44);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && window()->windowHandle()) {
            window()->windowHandle()->startSystemMove();
            event->accept();
            return;
        }
        QFrame::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
            event->accept();
            return;
        }
        QFrame::mouseDoubleClickEvent(event);
    }
};
}

class ResizeHandle : public QWidget
{
public:
    ResizeHandle(QWidget *parent, Qt::Edges edges)
        : QWidget(parent)
        , m_edges(edges)
    {
        if (edges == Qt::LeftEdge || edges == Qt::RightEdge) {
            setCursor(Qt::SizeHorCursor);
        } else if (edges == Qt::TopEdge || edges == Qt::BottomEdge) {
            setCursor(Qt::SizeVerCursor);
        } else if (edges == (Qt::TopEdge | Qt::LeftEdge) || edges == (Qt::BottomEdge | Qt::RightEdge)) {
            setCursor(Qt::SizeFDiagCursor);
        } else {
            setCursor(Qt::SizeBDiagCursor);
        }
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && window()->windowHandle()) {
            window()->windowHandle()->startSystemResize(m_edges);
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

private:
    Qt::Edges m_edges;
};

WaypaneWindow::WaypaneWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(tr("Waypane"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/icons/waypane.png")));
    setMinimumSize(940, 620);
    resize(1280, 800);

    QSettings settings;
    m_themeMode = static_cast<ThemeMode>(settings.value(QStringLiteral("appearance/theme"), 0).toInt());
    const QString lastProfileId = settings.value(QStringLiteral("workspace/selectedProfileId")).toString();
    ensureTerminalThemes();
    buildUi();
    applyTheme();
    refreshConnections();
    if (!lastProfileId.isEmpty()) {
        for (int row = 0; row < m_connections.rowCount(); ++row) {
            const auto profile = m_connections.profileAt(row);
            if (profile && profile->id == lastProfileId) {
                refreshConnections(row);
                break;
            }
        }
    }
    syncManagedSshConfig();
    restoreWorkspace();

    const QList<Qt::Edges> edges = {
        Qt::LeftEdge,
        Qt::RightEdge,
        Qt::TopEdge,
        Qt::BottomEdge,
        Qt::TopEdge | Qt::LeftEdge,
        Qt::TopEdge | Qt::RightEdge,
        Qt::BottomEdge | Qt::LeftEdge,
        Qt::BottomEdge | Qt::RightEdge,
    };
    for (Qt::Edges edge : edges) {
        m_resizeHandles.append(new ResizeHandle(this, edge));
    }
    updateResizeHandles();

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_themeMode == ThemeMode::System) {
            applyTheme();
        }
    });
}

WaypaneWindow::~WaypaneWindow()
{
    saveWorkspace();

    // QWidget destroys child terminal widgets from its base destructor, after
    // WaypaneWindow's members have already been destroyed. Disconnect their
    // bookkeeping callbacks while the session map is still alive.
    for (QWidget *terminalWidget : m_terminalSessions.keys()) {
        QObject::disconnect(terminalWidget, nullptr, this, nullptr);
        const TerminalSession session = m_terminalSessions.value(terminalWidget);
        if (session.part) {
            QObject::disconnect(session.part, nullptr, this, nullptr);
        }
    }
    m_terminalSessions.clear();
}

void WaypaneWindow::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);
    outer->setSpacing(0);

    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("windowFrame"));
    outer->addWidget(frame);
    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);

    auto *titleBar = new TitleBar(frame);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 0, 0);
    titleLayout->setSpacing(9);
    auto *icon = new QLabel(titleBar);
    icon->setPixmap(QIcon(QStringLiteral(":/assets/icons/waypane.png")).pixmap(25, 25));
    auto *name = new QLabel(tr("Waypane"), titleBar);
    name->setObjectName(QStringLiteral("appName"));
    auto *descriptor = new QLabel(tr("REMOTE WORKSPACE"), titleBar);
    descriptor->setObjectName(QStringLiteral("descriptor"));
    titleLayout->addWidget(icon);
    titleLayout->addWidget(name);
    titleLayout->addWidget(descriptor);
    titleLayout->addStretch();

    m_themeButton = new QToolButton(titleBar);
    m_themeButton->setObjectName(QStringLiteral("windowButton"));
    connect(m_themeButton, &QToolButton::clicked, this, &WaypaneWindow::cycleTheme);
    auto *settingsButton = new QToolButton(titleBar);
    settingsButton->setObjectName(QStringLiteral("windowButton"));
    settingsButton->setText(QStringLiteral("⚙"));
    settingsButton->setToolTip(tr("Settings"));
    connect(settingsButton, &QToolButton::clicked, this, &WaypaneWindow::openSettings);
    auto *minimize = new QToolButton(titleBar);
    minimize->setObjectName(QStringLiteral("windowButton"));
    minimize->setText(QStringLiteral("—"));
    connect(minimize, &QToolButton::clicked, this, &QWidget::showMinimized);
    auto *maximize = new QToolButton(titleBar);
    maximize->setObjectName(QStringLiteral("windowButton"));
    maximize->setText(QStringLiteral("□"));
    connect(maximize, &QToolButton::clicked, this, [this, maximize] {
        isMaximized() ? showNormal() : showMaximized();
        maximize->setText(isMaximized() ? QStringLiteral("❐") : QStringLiteral("□"));
    });
    auto *close = new QToolButton(titleBar);
    close->setObjectName(QStringLiteral("windowCloseButton"));
    close->setText(QStringLiteral("×"));
    connect(close, &QToolButton::clicked, this, &QWidget::close);
    for (QToolButton *button : {settingsButton, m_themeButton, minimize, maximize, close}) {
        button->setFixedSize(40, 42);
        titleLayout->addWidget(button);
    }
    frameLayout->addWidget(titleBar);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    frameLayout->addLayout(body, 1);

    auto *connectionsPane = new QFrame(frame);
    connectionsPane->setObjectName(QStringLiteral("connectionsPane"));
    connectionsPane->setFixedWidth(300);
    auto *connectionsLayout = new QVBoxLayout(connectionsPane);
    connectionsLayout->setContentsMargins(16, 16, 16, 16);
    connectionsLayout->setSpacing(10);
    auto *connectionsHeader = new QHBoxLayout;
    auto *connectionsLabel = new QLabel(tr("CONNECTIONS"), connectionsPane);
    connectionsLabel->setObjectName(QStringLiteral("sectionLabel"));
    auto *newButton = new QPushButton(tr("+  New"), connectionsPane);
    newButton->setObjectName(QStringLiteral("softButton"));
    connect(newButton, &QPushButton::clicked, this, &WaypaneWindow::addConnection);
    connectionsHeader->addWidget(connectionsLabel);
    connectionsHeader->addStretch();
    connectionsHeader->addWidget(newButton);
    connectionsLayout->addLayout(connectionsHeader);
    m_search = new QLineEdit(connectionsPane);
    m_search->setPlaceholderText(tr("Search servers, folders, tags"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &WaypaneWindow::filterConnections);
    connectionsLayout->addWidget(m_search);
    m_connectionList = new QTreeView(connectionsPane);
    m_connectionTreeModel = new QStandardItemModel(m_connectionList);
    m_connectionList->setModel(m_connectionTreeModel);
    m_connectionList->setHeaderHidden(true);
    m_connectionList->setIndentation(16);
    m_connectionList->setRootIsDecorated(true);
    m_connectionList->setUniformRowHeights(true);
    m_connectionList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_connectionList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_connectionList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_connectionList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &WaypaneWindow::updateSelection);
    connect(m_connectionList, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.data(Qt::UserRole).toInt() >= 0) {
            connectSelected();
        }
    });
    connect(m_connectionList, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        const QModelIndex index = m_connectionList->indexAt(position);
        bool rowOk = false;
        const int row = index.data(Qt::UserRole).toInt(&rowOk);
        if (!rowOk || row < 0) {
            return;
        }
        m_connectionList->setCurrentIndex(index);
        QMenu menu(this);
        menu.addAction(tr("Open SSH"), this, &WaypaneWindow::connectSelected);
        menu.addAction(tr("Open SFTP"), this, &WaypaneWindow::browseSelectedFiles);
        auto *tunnels = menu.addAction(tr("Start tunnels"), this, &WaypaneWindow::connectSelectedTunnels);
        const auto profile = selectedProfile();
        tunnels->setEnabled(profile && (!profile->localForwards.isEmpty() || !profile->remoteForwards.isEmpty() || !profile->dynamicForwards.isEmpty()));
        menu.addAction(tr("Configure port forwarding…"), this, &WaypaneWindow::configurePortForwards);
        menu.addSeparator();
        menu.addAction(tr("Edit connection"), this, &WaypaneWindow::editConnection);
        menu.exec(m_connectionList->viewport()->mapToGlobal(position));
    });
    connectionsLayout->addWidget(m_connectionList, 1);
    auto *profileActions = new QHBoxLayout;
    m_editButton = new QPushButton(tr("Edit"), connectionsPane);
    m_deleteButton = new QPushButton(tr("Delete"), connectionsPane);
    connect(m_editButton, &QPushButton::clicked, this, &WaypaneWindow::editConnection);
    connect(m_deleteButton, &QPushButton::clicked, this, &WaypaneWindow::deleteConnection);
    profileActions->addWidget(m_editButton);
    profileActions->addWidget(m_deleteButton);
    connectionsLayout->addLayout(profileActions);
    auto *importButton = new QPushButton(tr("Import OpenSSH config"), connectionsPane);
    connect(importButton, &QPushButton::clicked, this, &WaypaneWindow::importConnections);
    connectionsLayout->addWidget(importButton);
    body->addWidget(connectionsPane);

    auto *workspace = new QFrame(frame);
    workspace->setObjectName(QStringLiteral("workspace"));
    auto *workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);
    auto *workspaceHeader = new QFrame(workspace);
    workspaceHeader->setObjectName(QStringLiteral("workspaceHeader"));
    workspaceHeader->setFixedHeight(58);
    auto *workspaceHeaderLayout = new QHBoxLayout(workspaceHeader);
    workspaceHeaderLayout->setContentsMargins(18, 0, 18, 0);
    auto *titleStack = new QVBoxLayout;
    m_workspaceTitle = new QLabel(tr("Waypane home"), workspaceHeader);
    m_workspaceTitle->setObjectName(QStringLiteral("workspaceTitle"));
    m_workspaceEndpoint = new QLabel(tr("Select a connection, then open SSH or SFTP"), workspaceHeader);
    m_workspaceEndpoint->setObjectName(QStringLiteral("mutedLabel"));
    titleStack->addWidget(m_workspaceTitle);
    titleStack->addWidget(m_workspaceEndpoint);
    workspaceHeaderLayout->addLayout(titleStack);
    workspaceHeaderLayout->addStretch();
    auto *localButton = new QPushButton(tr("Local terminal"), workspaceHeader);
    connect(localButton, &QPushButton::clicked, this, &WaypaneWindow::newLocalTerminal);
    auto *splitButton = new QPushButton(tr("Split"), workspaceHeader);
    auto *splitMenu = new QMenu(splitButton);
    splitMenu->addAction(tr("Split right"), this, [this] {
        splitLocalTerminal(Qt::Horizontal);
    });
    splitMenu->addAction(tr("Split down"), this, [this] {
        splitLocalTerminal(Qt::Vertical);
    });
    splitButton->setMenu(splitMenu);
    splitButton->setToolTip(tr("Add a local terminal pane to the current tab"));
    m_sftpButton = new QPushButton(tr("Browse files"), workspaceHeader);
    connect(m_sftpButton, &QPushButton::clicked, this, &WaypaneWindow::browseSelectedFiles);
    m_connectButton = new QPushButton(tr("SSH"), workspaceHeader);
    m_connectButton->setObjectName(QStringLiteral("primaryButton"));
    connect(m_connectButton, &QPushButton::clicked, this, &WaypaneWindow::connectSelected);
    workspaceHeaderLayout->addWidget(localButton);
    workspaceHeaderLayout->addWidget(splitButton);
    workspaceHeaderLayout->addWidget(m_sftpButton);
    workspaceHeaderLayout->addWidget(m_connectButton);
    workspaceLayout->addWidget(workspaceHeader);

    m_workspaceSplit = new QSplitter(Qt::Horizontal, workspace);
    m_terminalStack = new QStackedWidget(m_workspaceSplit);
    m_terminalStack->setObjectName(QStringLiteral("terminalStack"));
    m_landingPage = new QFrame(m_terminalStack);
    m_landingPage->setObjectName(QStringLiteral("landingPage"));
    auto *landingLayout = new QVBoxLayout(m_landingPage);
    landingLayout->setContentsMargins(54, 44, 54, 44);
    landingLayout->setSpacing(18);
    auto *welcome = new QLabel(tr("Your remote work starts here"), m_landingPage);
    welcome->setObjectName(QStringLiteral("landingTitle"));
    auto *welcomeText = new QLabel(tr("Choose a saved connection, start a clean local shell, or bring existing OpenSSH hosts into Waypane."), m_landingPage);
    welcomeText->setObjectName(QStringLiteral("landingSubtitle"));
    welcomeText->setWordWrap(true);
    landingLayout->addStretch(1);
    landingLayout->addWidget(welcome);
    landingLayout->addWidget(welcomeText);
    auto *quickActions = new QHBoxLayout;
    quickActions->setSpacing(10);
    auto *landingSsh = new QPushButton(tr("Open selected host"), m_landingPage);
    landingSsh->setObjectName(QStringLiteral("primaryButton"));
    connect(landingSsh, &QPushButton::clicked, this, &WaypaneWindow::connectSelected);
    auto *landingLocal = new QPushButton(tr("New local terminal"), m_landingPage);
    connect(landingLocal, &QPushButton::clicked, this, &WaypaneWindow::newLocalTerminal);
    auto *landingImport = new QPushButton(tr("Import OpenSSH config"), m_landingPage);
    connect(landingImport, &QPushButton::clicked, this, &WaypaneWindow::importConnections);
    quickActions->addWidget(landingSsh);
    quickActions->addWidget(landingLocal);
    quickActions->addWidget(landingImport);
    quickActions->addStretch();
    landingLayout->addLayout(quickActions);

    auto *cards = new QGridLayout;
    cards->setSpacing(10);
    auto addLandingCard = [this, cards](int column, const QString &heading, const QString &detail) {
        auto *card = new QFrame(m_landingPage);
        card->setObjectName(QStringLiteral("landingCard"));
        auto *cardLayout = new QVBoxLayout(card);
        auto *cardTitle = new QLabel(heading, card);
        cardTitle->setObjectName(QStringLiteral("cardTitle"));
        auto *cardDetail = new QLabel(detail, card);
        cardDetail->setObjectName(QStringLiteral("mutedLabel"));
        cardDetail->setWordWrap(true);
        cardLayout->addWidget(cardTitle);
        cardLayout->addWidget(cardDetail);
        cards->addWidget(card, 0, column);
    };
    addLandingCard(0, tr("SSH workspaces"), tr("Open terminals without starting SFTP. Split panes only when the task needs them."));
    addLandingCard(1, tr("Files on demand"), tr("Open the local/remote file workspace from a host's context menu."));
    addLandingCard(2, tr("Auditable sessions"), tr("Optional per-host UTC logs can be enabled in Settings."));
    landingLayout->addLayout(cards);
    m_hostCountLabel = new QLabel(m_landingPage);
    m_hostCountLabel->setObjectName(QStringLiteral("landingMeta"));
    landingLayout->addWidget(m_hostCountLabel);
    landingLayout->addStretch(2);
    m_terminalStack->addWidget(m_landingPage);

    m_terminalTabs = new QTabWidget(m_terminalStack);
    m_terminalTabs->setObjectName(QStringLiteral("terminalTabs"));
    m_terminalTabs->setTabsClosable(true);
    m_terminalTabs->setMovable(true);
    connect(m_terminalTabs, &QTabWidget::tabCloseRequested, this, &WaypaneWindow::closeTerminal);
    auto *closeShortcut = new QShortcut(QKeySequence::Close, this);
    closeShortcut->setContext(Qt::WindowShortcut);
    connect(closeShortcut, &QShortcut::activated, this, [this] {
        closeTerminal(m_terminalTabs->currentIndex());
    });
    m_terminalStack->addWidget(m_terminalTabs);
    m_terminalStack->setCurrentWidget(m_landingPage);
    m_fileStack = new QStackedWidget(m_workspaceSplit);
    m_fileStack->setObjectName(QStringLiteral("fileStack"));
    m_fileBrowserPage = new QFrame(m_fileStack);
    auto *fileBrowserLayout = new QVBoxLayout(m_fileBrowserPage);
    fileBrowserLayout->setContentsMargins(0, 0, 0, 0);
    fileBrowserLayout->setSpacing(0);
    auto *fileToolbar = new QFrame(m_fileBrowserPage);
    fileToolbar->setObjectName(QStringLiteral("fileToolbar"));
    auto *fileToolbarLayout = new QHBoxLayout(fileToolbar);
    fileToolbarLayout->setContentsMargins(8, 6, 8, 6);
    fileToolbarLayout->setSpacing(5);
    auto *fileTitle = new QLabel(tr("LOCAL ↔ REMOTE"), fileToolbar);
    fileTitle->setObjectName(QStringLiteral("sectionLabel"));
    m_transferStatus = new QLabel(tr("rsync preferred"), fileToolbar);
    m_transferStatus->setObjectName(QStringLiteral("mutedLabel"));
    auto *closeFiles = new QToolButton(fileToolbar);
    closeFiles->setText(QStringLiteral("×"));
    closeFiles->setToolTip(tr("Close file workspace"));
    fileToolbarLayout->addWidget(fileTitle);
    fileToolbarLayout->addWidget(m_transferStatus);
    fileToolbarLayout->addStretch();
    fileToolbarLayout->addWidget(closeFiles);
    fileBrowserLayout->addWidget(fileToolbar);

    auto *fileSplit = new QSplitter(Qt::Horizontal, m_fileBrowserPage);
    auto *localPane = new QFrame(fileSplit);
    auto *localLayout = new QVBoxLayout(localPane);
    localLayout->setContentsMargins(0, 0, 0, 0);
    localLayout->setSpacing(0);
    auto *localToolbar = new QFrame(localPane);
    localToolbar->setObjectName(QStringLiteral("fileToolbar"));
    auto *localToolbarLayout = new QHBoxLayout(localToolbar);
    localToolbarLayout->setContentsMargins(8, 6, 8, 6);
    auto *localUp = new QToolButton(localToolbar);
    localUp->setText(QStringLiteral("↑"));
    localUp->setToolTip(tr("Parent folder"));
    auto *localReload = new QToolButton(localToolbar);
    localReload->setText(QStringLiteral("↻"));
    m_localPath = new QLineEdit(localToolbar);
    m_localPath->setPlaceholderText(tr("Local path"));
    auto *uploadButton = new QToolButton(localToolbar);
    uploadButton->setText(QStringLiteral("→"));
    uploadButton->setToolTip(tr("Copy selected local files to the remote folder (rsync preferred)"));
    localToolbarLayout->addWidget(localUp);
    localToolbarLayout->addWidget(localReload);
    localToolbarLayout->addWidget(m_localPath, 1);
    localToolbarLayout->addWidget(uploadButton);
    localLayout->addWidget(localToolbar);
    m_localFileBrowser = new KDirOperator(QUrl::fromLocalFile(QDir::homePath()), localPane);
    m_localFileBrowser->setViewMode(KFile::Detail);
    m_localFileBrowser->setMode(KFile::Files | KFile::Directory | KFile::ExistingOnly | KFile::LocalOnly);
    m_localFileBrowser->view()->viewport()->setAcceptDrops(true);
    m_localFileBrowser->view()->viewport()->installEventFilter(this);
    localLayout->addWidget(m_localFileBrowser, 1);

    auto *remotePane = new QFrame(fileSplit);
    auto *remoteLayout = new QVBoxLayout(remotePane);
    remoteLayout->setContentsMargins(0, 0, 0, 0);
    remoteLayout->setSpacing(0);
    auto *remoteToolbar = new QFrame(remotePane);
    remoteToolbar->setObjectName(QStringLiteral("fileToolbar"));
    auto *remoteToolbarLayout = new QHBoxLayout(remoteToolbar);
    remoteToolbarLayout->setContentsMargins(8, 6, 8, 6);
    auto *upButton = new QToolButton(remoteToolbar);
    upButton->setText(QStringLiteral("↑"));
    upButton->setToolTip(tr("Parent folder"));
    auto *reloadButton = new QToolButton(remoteToolbar);
    reloadButton->setText(QStringLiteral("↻"));
    reloadButton->setToolTip(tr("Refresh"));
    auto *newFolderButton = new QToolButton(remoteToolbar);
    newFolderButton->setText(QStringLiteral("＋"));
    newFolderButton->setToolTip(tr("New folder"));
    m_remotePath = new QLineEdit(remoteToolbar);
    m_remotePath->setPlaceholderText(tr("Remote path"));
    auto *editRemoteButton = new QToolButton(remoteToolbar);
    editRemoteButton->setText(QStringLiteral("✎"));
    editRemoteButton->setToolTip(tr("Edit the selected remote text file"));
    auto *downloadButton = new QToolButton(remoteToolbar);
    downloadButton->setText(QStringLiteral("←"));
    downloadButton->setToolTip(tr("Copy selected remote files to the local folder (rsync preferred)"));
    remoteToolbarLayout->addWidget(downloadButton);
    remoteToolbarLayout->addWidget(upButton);
    remoteToolbarLayout->addWidget(reloadButton);
    remoteToolbarLayout->addWidget(newFolderButton);
    remoteToolbarLayout->addWidget(editRemoteButton);
    remoteToolbarLayout->addWidget(m_remotePath, 1);
    remoteLayout->addWidget(remoteToolbar);

    m_fileBrowser = new KDirOperator(QUrl(), remotePane);
    m_fileBrowser->setObjectName(QStringLiteral("fileBrowser"));
    m_fileBrowser->setViewMode(KFile::Detail);
    m_fileBrowser->setMode(KFile::Files | KFile::Directory | KFile::ExistingOnly);
    m_fileBrowser->view()->viewport()->setAcceptDrops(true);
    m_fileBrowser->view()->viewport()->installEventFilter(this);
    remoteLayout->addWidget(m_fileBrowser, 1);
    fileSplit->addWidget(localPane);
    fileSplit->addWidget(remotePane);
    fileSplit->setSizes({360, 360});
    fileBrowserLayout->addWidget(fileSplit, 1);
    connect(closeFiles, &QToolButton::clicked, this, &WaypaneWindow::closeFilePanel);
    connect(localUp, &QToolButton::clicked, m_localFileBrowser, &KDirOperator::cdUp);
    connect(localReload, &QToolButton::clicked, m_localFileBrowser, &KDirOperator::rereadDir);
    connect(upButton, &QToolButton::clicked, m_fileBrowser, &KDirOperator::cdUp);
    connect(reloadButton, &QToolButton::clicked, m_fileBrowser, &KDirOperator::rereadDir);
    connect(newFolderButton, &QToolButton::clicked, m_fileBrowser, &KDirOperator::mkdir);
    connect(uploadButton, &QToolButton::clicked, this, &WaypaneWindow::uploadFiles);
    connect(downloadButton, &QToolButton::clicked, this, &WaypaneWindow::downloadSelected);
    connect(editRemoteButton, &QToolButton::clicked, this, &WaypaneWindow::editRemoteSelected);
    connect(m_localPath, &QLineEdit::returnPressed, this, [this] {
        m_localFileBrowser->setUrl(QUrl::fromLocalFile(m_localPath->text().trimmed()), true);
    });
    connect(m_localFileBrowser, &KDirOperator::urlEntered, this, [this](const QUrl &url) {
        m_localPath->setText(url.toLocalFile());
    });
    connect(m_remotePath, &QLineEdit::returnPressed, this, [this] {
        QUrl url = m_fileBrowser->url();
        url.setPath(m_remotePath->text().trimmed());
        m_fileBrowser->setUrl(url, true);
    });
    connect(m_fileBrowser, &KDirOperator::urlEntered, this, [this](const QUrl &url) {
        m_remotePath->setText(url.path());
    });
    m_fileStack->addWidget(m_fileBrowserPage);

    m_remoteEditorPage = new QFrame(m_fileStack);
    auto *editorLayout = new QVBoxLayout(m_remoteEditorPage);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    auto *editorToolbar = new QFrame(m_remoteEditorPage);
    editorToolbar->setObjectName(QStringLiteral("fileToolbar"));
    auto *editorToolbarLayout = new QHBoxLayout(editorToolbar);
    editorToolbarLayout->setContentsMargins(8, 6, 8, 6);
    auto *backToFiles = new QToolButton(editorToolbar);
    backToFiles->setText(QStringLiteral("←"));
    backToFiles->setToolTip(tr("Back to local and remote files"));
    m_remoteEditorPath = new QLabel(editorToolbar);
    m_remoteEditorPath->setObjectName(QStringLiteral("mutedLabel"));
    auto *saveRemote = new QPushButton(tr("Save remote file"), editorToolbar);
    saveRemote->setObjectName(QStringLiteral("primaryButton"));
    editorToolbarLayout->addWidget(backToFiles);
    editorToolbarLayout->addWidget(m_remoteEditorPath, 1);
    editorToolbarLayout->addWidget(saveRemote);
    editorLayout->addWidget(editorToolbar);
    m_remoteEditor = new QPlainTextEdit(m_remoteEditorPage);
    m_remoteEditor->setObjectName(QStringLiteral("remoteEditor"));
    m_remoteEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editorLayout->addWidget(m_remoteEditor, 1);
    connect(backToFiles, &QToolButton::clicked, this, [this] {
        if (confirmDiscardRemoteEdit()) {
            m_fileStack->setCurrentWidget(m_fileBrowserPage);
        }
    });
    connect(saveRemote, &QPushButton::clicked, this, &WaypaneWindow::saveRemoteEdit);
    m_fileStack->addWidget(m_remoteEditorPage);
    m_fileStack->setCurrentWidget(m_fileBrowserPage);
    m_fileStack->hide();
    m_workspaceSplit->addWidget(m_terminalStack);
    m_workspaceSplit->addWidget(m_fileStack);
    m_workspaceSplit->setStretchFactor(0, 1);
    m_workspaceSplit->setStretchFactor(1, 1);
    workspaceLayout->addWidget(m_workspaceSplit, 1);
    body->addWidget(workspace, 1);
}

void WaypaneWindow::applyTheme()
{
    const bool dark = isDarkTheme();
    const QString ink = dark ? QStringLiteral("#0b1014") : QStringLiteral("#f4f7f6");
    const QString panel = dark ? QStringLiteral("#121a20") : QStringLiteral("#e9efed");
    const QString raised = dark ? QStringLiteral("#182229") : QStringLiteral("#ffffff");
    const QString line = dark ? QStringLiteral("#26343c") : QStringLiteral("#c9d3d0");
    const QString text = dark ? QStringLiteral("#e8f0ee") : QStringLiteral("#17201f");
    const QString muted = dark ? QStringLiteral("#82918f") : QStringLiteral("#657370");
    const QString jade = dark ? QStringLiteral("#55d6a7") : QStringLiteral("#087f61");
    const QString jadeSoft = dark ? QStringLiteral("#18362f") : QStringLiteral("#d7eee7");
    const QString hover = dark ? QStringLiteral("#203039") : QStringLiteral("#dfe8e5");
    const QString title = dark ? QStringLiteral("#0e1519") : QStringLiteral("#f8faf9");

    QPalette palette;
    palette.setColor(QPalette::Window, ink);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, raised);
    palette.setColor(QPalette::AlternateBase, panel);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, raised);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::Highlight, jade);
    palette.setColor(QPalette::HighlightedText, dark ? ink : QStringLiteral("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, muted);
    qApp->setPalette(palette);

    qApp->setStyleSheet(QStringLiteral(R"(
        * { color: %1; }
        #windowFrame, #dialogFrame { background: %2; border: 1px solid %3; border-radius: 10px; }
        #titleBar { background: %4; border-bottom: 1px solid %3; }
        #appName, #workspaceTitle, #dialogTitle, #cardTitle { font-weight: 600; }
        #descriptor, #sectionLabel { color: %5; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
        #mutedLabel, #versionLabel { color: %5; font-size: 11px; }
        #connectionsPane { background: %6; border-right: 1px solid %3; }
        #workspace { background: %2; }
        #workspaceHeader { background: %4; border-bottom: 1px solid %3; }
        #landingPage { background: %2; }
        #landingTitle { font-size: 26px; font-weight: 650; }
        #landingSubtitle { color: %5; font-size: 13px; }
        #landingMeta { color: %5; font-size: 11px; padding-top: 4px; }
        #landingCard { background: %6; border: 1px solid %3; border-radius: 12px; }
        QLineEdit, QSpinBox, QComboBox { background: %7; border: 1px solid %3; border-radius: 8px; padding: 8px; selection-background-color: %8; }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: %8; }
        QPushButton, QToolButton { background: %7; border: 1px solid %3; border-radius: 8px; padding: 7px 12px; }
        QPushButton:hover, QToolButton:hover { background: %9; }
        QPushButton:focus, QToolButton:focus { border-color: %8; }
        QPushButton:disabled { color: %5; }
        #primaryButton { background: %10; border-color: %8; color: %1; font-weight: 600; }
        #windowButton, #windowCloseButton { background: transparent; border: 0; border-radius: 7px; padding: 0; font-size: 17px; }
        #windowCloseButton:hover { background: #d94a55; color: white; }
        QTreeView { background: transparent; border: 0; outline: 0; padding: 2px; }
        QTreeView::item { border-radius: 8px; padding: 8px; margin: 1px 0; }
        QTreeView::item:hover { background: %9; }
        QTreeView::item:selected { background: %10; color: %1; }
        QTreeView::branch:selected { background: %10; }
        QScrollArea#profileScroll, QScrollArea#profileScroll > QWidget > QWidget { background: transparent; }
        QTabWidget::pane { border: 0; background: %2; }
        QTabBar::tab { background: %6; border-right: 1px solid %3; padding: 8px 14px; }
        QTabBar::tab:selected { background: %2; color: %8; }
        QPlainTextEdit#remoteEditor { background: %7; border: 0; padding: 10px; font-family: "JetBrains Mono", "Noto Sans Mono", monospace; }
        QSplitter::handle { background: %3; width: 1px; }
        QToolButton::menu-indicator { image: none; }
        QScrollBar { background: transparent; width: 10px; height: 10px; }
        QScrollBar::handle { background: %3; border-radius: 5px; min-height: 24px; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
    )")
                            .arg(text, ink, line, title, muted, panel, raised, jade, hover, jadeSoft));

    if (m_themeButton) {
        switch (m_themeMode) {
        case ThemeMode::System:
            m_themeButton->setText(QStringLiteral("◐"));
            m_themeButton->setToolTip(tr("Theme: system"));
            break;
        case ThemeMode::Light:
            m_themeButton->setText(QStringLiteral("☀"));
            m_themeButton->setToolTip(tr("Theme: light"));
            break;
        case ThemeMode::Dark:
            m_themeButton->setText(QStringLiteral("☾"));
            m_themeButton->setToolTip(tr("Theme: dark"));
            break;
        }
    }
    applyTerminalThemes();
}

void WaypaneWindow::ensureTerminalThemes() const
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/konsole");
    if (!QDir().mkpath(directory)) {
        return;
    }
    const auto writeManagedFile = [](const QString &path, const QByteArray &contents) {
        QFile current(path);
        if (current.open(QIODevice::ReadOnly) && current.readAll() == contents) {
            return;
        }
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly)) {
            return;
        }
        output.write(contents);
        output.commit();
    };
    const auto colorScheme = [](const QString &description, const QStringList &normal, const QStringList &intense, const QString &background, const QString &foreground) {
        QByteArray result = "[Background]\nColor=" + background.toUtf8() + "\n\n[BackgroundIntense]\nColor=" + background.toUtf8()
            + "\n\n[Foreground]\nColor=" + foreground.toUtf8() + "\n\n[ForegroundIntense]\nColor=" + foreground.toUtf8() + '\n';
        for (int index = 0; index < 8; ++index) {
            result += "\n[Color" + QByteArray::number(index) + "]\nColor=" + normal.at(index).toUtf8() + '\n';
            result += "\n[Color" + QByteArray::number(index) + "Intense]\nColor=" + intense.at(index).toUtf8() + '\n';
        }
        result += "\n[General]\nDescription=" + description.toUtf8() + "\nOpacity=1\nWallpaper=\n";
        return result;
    };
    const QStringList night = {QStringLiteral("11,16,20"), QStringLiteral("240,104,116"), QStringLiteral("85,214,167"), QStringLiteral("238,194,96"),
                               QStringLiteral("94,164,255"), QStringLiteral("194,130,255"), QStringLiteral("77,205,220"), QStringLiteral("214,225,222")};
    const QStringList nightBright = {QStringLiteral("86,101,108"), QStringLiteral("255,128,138"), QStringLiteral("115,232,186"), QStringLiteral("255,215,124"),
                                     QStringLiteral("126,184,255"), QStringLiteral("215,160,255"), QStringLiteral("104,225,237"), QStringLiteral("248,251,250")};
    const QStringList day = {QStringLiteral("27,36,35"), QStringLiteral("183,50,65"), QStringLiteral("8,127,97"), QStringLiteral("151,105,0"),
                             QStringLiteral("31,91,178"), QStringLiteral("126,65,160"), QStringLiteral("0,119,139"), QStringLiteral("225,232,230")};
    const QStringList dayBright = {QStringLiteral("91,105,102"), QStringLiteral("210,62,77"), QStringLiteral("10,151,115"), QStringLiteral("178,124,0"),
                                   QStringLiteral("45,111,204"), QStringLiteral("148,78,186"), QStringLiteral("0,143,165"), QStringLiteral("255,255,255")};
    writeManagedFile(QDir(directory).filePath(QStringLiteral("WaypaneNight.colorscheme")),
                     colorScheme(QStringLiteral("Waypane Night"), night, nightBright, QStringLiteral("11,16,20"), QStringLiteral("232,240,238")));
    writeManagedFile(QDir(directory).filePath(QStringLiteral("WaypaneDay.colorscheme")),
                     colorScheme(QStringLiteral("Waypane Day"), day, dayBright, QStringLiteral("247,250,249"), QStringLiteral("23,32,31")));
    writeManagedFile(QDir(directory).filePath(QStringLiteral("Waypane Night.profile")),
                     QByteArrayLiteral("[Appearance]\nColorScheme=WaypaneNight\n\n[General]\nName=Waypane Night\nParent=FALLBACK/\n"));
    writeManagedFile(QDir(directory).filePath(QStringLiteral("Waypane Day.profile")),
                     QByteArrayLiteral("[Appearance]\nColorScheme=WaypaneDay\n\n[General]\nName=Waypane Day\nParent=FALLBACK/\n"));
}

void WaypaneWindow::applyTerminalTheme(TerminalInterface *terminal) const
{
    if (!terminal) {
        return;
    }
    const QString preference = QSettings().value(QStringLiteral("terminal/theme"), QStringLiteral("auto")).toString();
    const bool night = preference == QStringLiteral("night") || (preference == QStringLiteral("auto") && isDarkTheme());
    terminal->setCurrentProfile(night ? QStringLiteral("Waypane Night") : QStringLiteral("Waypane Day"));
}

void WaypaneWindow::applyTerminalThemes() const
{
    for (const TerminalSession &session : m_terminalSessions) {
        applyTerminalTheme(session.interface);
    }
}

QString WaypaneWindow::helperExecutable(const QString &name) const
{
    QString path = QCoreApplication::applicationDirPath() + QLatin1Char('/') + name;
    if (!QFileInfo::exists(path)) {
        path = QCoreApplication::applicationDirPath() + QStringLiteral("/../libexec/") + name;
    }
    return QFileInfo::exists(path) ? QFileInfo(path).absoluteFilePath() : QString();
}

QString WaypaneWindow::sessionLogPath(const Waypane::ConnectionProfile &profile) const
{
    QSettings settings;
    if (!settings.value(QStringLiteral("logging/enabled"), false).toBool()) {
        return {};
    }
    QString root = settings.value(QStringLiteral("logging/directory")).toString();
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (root.isEmpty()) {
            root = QDir::homePath();
        }
        root = QDir(root).filePath(QStringLiteral("Waypane Logs"));
    }
    QString host = profile.host.trimmed();
    host.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("_"));
    if (host.isEmpty()) {
        host = QStringLiteral("unknown-host");
    }
    const QString directory = QDir(root).filePath(host);
    if (!QDir().mkpath(directory)) {
        return {};
    }
    QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    const QString utc = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmss.zzz'Z'"));
    const QString suffix = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    return QDir(directory).filePath(QStringLiteral("%1_%2_%3.log").arg(host, utc, suffix));
}

void WaypaneWindow::refreshConnections(int selectRow)
{
    m_connectionTreeModel->clear();
    if (m_hostCountLabel) {
        m_hostCountLabel->setText(tr("%n saved host(s) · Search includes names, addresses, folders, and tags", nullptr, m_connections.rowCount()));
    }
    QHash<QString, QStandardItem *> folders;
    QStandardItem *root = m_connectionTreeModel->invisibleRootItem();
    QModelIndex requested;
    QModelIndex firstProfile;

    for (int row = 0; row < m_connections.rowCount(); ++row) {
        const auto profile = m_connections.profileAt(row);
        if (!profile) {
            continue;
        }
        QStandardItem *parent = root;
        QString folderKey;
        const QStringList parts = profile->group.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        for (QString part : parts) {
            part = part.trimmed();
            if (part.isEmpty()) {
                continue;
            }
            folderKey += QLatin1Char('/') + part;
            QStandardItem *folder = folders.value(folderKey);
            if (!folder) {
                folder = new QStandardItem(part);
                folder->setEditable(false);
                folder->setData(-1, Qt::UserRole);
                parent->appendRow(folder);
                folders.insert(folderKey, folder);
            }
            parent = folder;
        }
        auto *item = new QStandardItem(profile->name);
        item->setEditable(false);
        item->setData(row, Qt::UserRole);
        item->setData(profile->id, Qt::UserRole + 1);
        const QString endpoint = (profile->username.isEmpty() ? QString() : profile->username + QLatin1Char('@')) + profile->host + QLatin1Char(':')
            + QString::number(profile->port);
        item->setToolTip(endpoint + (profile->tags.isEmpty() ? QString() : QStringLiteral("\nTags: ") + profile->tags.join(QStringLiteral(", "))));
        parent->appendRow(item);
        const QModelIndex index = item->index();
        if (!firstProfile.isValid()) {
            firstProfile = index;
        }
        if (row == selectRow) {
            requested = index;
        }
    }
    m_connectionList->expandToDepth(0);
    m_connectionList->setCurrentIndex(requested.isValid() ? requested : firstProfile);
    filterConnections(m_search->text());
    updateSelection();
}

std::optional<Waypane::ConnectionProfile> WaypaneWindow::selectedProfile() const
{
    bool ok = false;
    const int row = m_connectionList->currentIndex().data(Qt::UserRole).toInt(&ok);
    return ok && row >= 0 ? m_connections.profileAt(row) : std::nullopt;
}

bool WaypaneWindow::connectToProfileId(const QString &profileId)
{
    for (int row = 0; row < m_connections.rowCount(); ++row) {
        const auto profile = m_connections.profileAt(row);
        if (profile && profile->id == profileId) {
            refreshConnections(row);
            connectSelected();
            return true;
        }
    }
    return false;
}

bool WaypaneWindow::openTunnelsProfileId(const QString &profileId)
{
    for (int row = 0; row < m_connections.rowCount(); ++row) {
        const auto profile = m_connections.profileAt(row);
        if (profile && profile->id == profileId) {
            refreshConnections(row);
            connectSelectedTunnels();
            return true;
        }
    }
    return false;
}

bool WaypaneWindow::browseProfileFilesById(const QString &profileId)
{
    for (int row = 0; row < m_connections.rowCount(); ++row) {
        const auto profile = m_connections.profileAt(row);
        if (profile && profile->id == profileId) {
            refreshConnections(row);
            browseSelectedFiles();
            return true;
        }
    }
    return false;
}

bool WaypaneWindow::editRemoteFileById(const QString &profileId, const QString &remotePath)
{
    if (!browseProfileFilesById(profileId)) {
        return false;
    }
    QUrl url = m_fileBrowser->url();
    url.setPath(remotePath.startsWith(QLatin1Char('/')) ? remotePath : QLatin1Char('/') + remotePath);
    openRemoteEditor(url);
    return true;
}

void WaypaneWindow::openLocalTerminal()
{
    newLocalTerminal();
}

QJsonObject WaypaneWindow::runtimeStatus() const
{
    return {
        {QStringLiteral("applicationVersion"), QCoreApplication::applicationVersion()},
        {QStringLiteral("profileSchemaVersion"), WAYPANE_PROFILE_SCHEMA_VERSION},
        {QStringLiteral("mcpProtocolVersion"), WAYPANE_MCP_PROTOCOL_VERSION},
        {QStringLiteral("running"), true},
        {QStringLiteral("activeProfileId"), m_activeProfileId},
        {QStringLiteral("terminalCount"), m_terminalSessions.size()},
        {QStringLiteral("terminalTabCount"), m_terminalTabs ? m_terminalTabs->count() : 0},
        {QStringLiteral("connectionCount"), m_connections.rowCount()},
        {QStringLiteral("fileWorkspaceOpen"), m_fileStack && m_fileStack->isVisible()},
        {QStringLiteral("remoteEditorOpen"), m_fileStack && m_fileStack->currentWidget() == m_remoteEditorPage},
        {QStringLiteral("transferActive"), m_transferActive},
        {QStringLiteral("queuedTransferCount"), m_transferQueue.size()},
        {QStringLiteral("sshExecutable"), Waypane::SshRuntime::executable()},
        {QStringLiteral("privateSshRuntime"), Waypane::SshRuntime::isPrivateRuntime()},
    };
}

WaypaneWindow::TerminalSession WaypaneWindow::createTerminal(const QString &title, QSplitter *workspace, const QString &profileId)
{
    const KPluginMetaData metadata(QStringLiteral("kf6/parts/konsolepart"), KPluginMetaData::AllowEmptyMetaData);
    const auto result = KPluginFactory::instantiatePlugin<KParts::ReadOnlyPart>(metadata, this);
    if (!result) {
        QMessageBox::critical(this, tr("Terminal unavailable"), tr("Could not load KonsolePart: %1").arg(result.errorString));
        return {};
    }
    TerminalInterface *terminal = qobject_cast<TerminalInterface *>(result.plugin);
    if (!terminal) {
        delete result.plugin;
        QMessageBox::critical(this, tr("Terminal unavailable"), tr("The installed KonsolePart does not provide TerminalInterface."));
        return {};
    }
    QWidget *terminalWidget = result.plugin->widget();
    if (!workspace) {
        workspace = new QSplitter(Qt::Horizontal, m_terminalTabs);
        workspace->setChildrenCollapsible(false);
        const int tab = m_terminalTabs->addTab(workspace, title);
        m_terminalTabs->setCurrentIndex(tab);
    }
    workspace->addWidget(terminalWidget);
    workspace->setSizes(QList<int>(workspace->count(), 1));
    applyTerminalTheme(terminal);
    m_terminalStack->setCurrentWidget(m_terminalTabs);
    terminalWidget->setFocus();
    TerminalSession session{result.plugin, terminal, profileId};
    m_terminalSessions.insert(terminalWidget, session);
    connect(terminalWidget, &QObject::destroyed, this, [this, terminalWidget] {
        m_terminalSessions.remove(terminalWidget);
    });
    const QPointer<QWidget> workspaceGuard(workspace);
    connect(result.plugin, &QObject::destroyed, this, [this, workspaceGuard] {
        // KonsolePart deletes itself after its terminal process exits (for
        // example Ctrl+D). Close the owning Waypane tab after that teardown.
        QTimer::singleShot(0, this, [this, workspaceGuard] {
            if (!workspaceGuard || !m_terminalTabs) {
                return;
            }
            const QList<QWidget *> liveDescendants = workspaceGuard->findChildren<QWidget *>();
            for (QWidget *terminalWidget : m_terminalSessions.keys()) {
                if (terminalWidget == workspaceGuard || liveDescendants.contains(terminalWidget)) {
                    return;
                }
            }
            const int tab = m_terminalTabs->indexOf(workspaceGuard);
            if (tab >= 0) {
                closeTerminal(tab);
            }
        });
    });
    return session;
}

bool WaypaneWindow::openSshSession(const Waypane::ConnectionProfile &profile, QSplitter *workspace, bool tunnelsOnly)
{
    if (profile.legacyCompatibility && !Waypane::SshRuntime::isPrivateRuntime()) {
        QMessageBox::warning(this,
                             tr("Private SSH runtime missing"),
                             tr("This profile requests legacy cryptography, but Waypane's isolated OpenSSH runtime is not installed. Build or package it with tools/build-private-openssh; the system SSH client will not be used for this connection."));
        return false;
    }
    const QString sshHelper = profile.secretId.isEmpty() ? QString() : helperExecutable(QStringLiteral("waypane-ssh-helper"));
    if (!profile.secretId.isEmpty() && sshHelper.isEmpty()) {
        QMessageBox::warning(this, tr("SSH helper missing"), tr("Waypane's credential helper is not installed, so the saved secret cannot be used."));
        return false;
    }
    const QString sessionRunner = helperExecutable(QStringLiteral("waypane-ssh-runner"));
    if (sessionRunner.isEmpty()) {
        QMessageBox::warning(this, tr("SSH session unavailable"), tr("Waypane's SSH session supervisor is not installed."));
        return false;
    }
    const bool loggingEnabled = !tunnelsOnly && QSettings().value(QStringLiteral("logging/enabled"), false).toBool();
    const QString logPath = loggingEnabled ? sessionLogPath(profile) : QString();
    const QString logger = loggingEnabled ? helperExecutable(QStringLiteral("waypane-session-logger")) : QString();
    if (loggingEnabled && (logPath.isEmpty() || logger.isEmpty())) {
        QMessageBox::warning(this, tr("Session logging unavailable"), tr("The SSH session was not started because its required audit log could not be created. Check the log destination in Settings."));
        return false;
    }
    TerminalSession session = createTerminal(tunnelsOnly ? tr("%1 tunnels").arg(profile.name) : profile.name,
                                             workspace,
                                             tunnelsOnly ? QStringLiteral("__transient__") : profile.id);
    if (!session.interface) {
        return false;
    }
    QStringList arguments = Waypane::SshCommandBuilder::arguments(profile, tunnelsOnly);
    const QString sshExecutable = Waypane::SshRuntime::executable();
    QString program;
    QStringList programArguments;
    if (profile.secretId.isEmpty()) {
        arguments.prepend(QStringLiteral("ssh"));
        program = sshExecutable;
        programArguments = arguments;
    } else {
        QStringList helperArguments{QStringLiteral("waypane-ssh-helper"), QStringLiteral("--ssh-executable"), sshExecutable, QStringLiteral("--secret-id"), profile.secretId, QStringLiteral("--")};
        helperArguments.append(arguments);
        program = sshHelper;
        programArguments = helperArguments;
    }
    if (loggingEnabled) {
        QStringList loggerArguments{QStringLiteral("waypane-session-logger"), QStringLiteral("--log"), logPath, QStringLiteral("--host"), profile.host, QStringLiteral("--"), program};
        loggerArguments.append(programArguments);
        program = logger;
        programArguments = loggerArguments;
        m_workspaceEndpoint->setToolTip(tr("Session log: %1").arg(logPath));
    }
    QStringList runnerArguments{QStringLiteral("waypane-ssh-runner"),
                                QStringLiteral("--profile"),
                                profile.name,
                                QStringLiteral("--legacy-enabled"),
                                profile.legacyCompatibility ? QStringLiteral("true") : QStringLiteral("false"),
                                QStringLiteral("--"),
                                program};
    runnerArguments.append(programArguments);
    session.interface->startProgram(sessionRunner, runnerArguments);
    return true;
}

void WaypaneWindow::splitLocalTerminal(Qt::Orientation orientation)
{
    auto *workspace = qobject_cast<QSplitter *>(m_terminalTabs->currentWidget());
    if (!workspace) {
        newLocalTerminal();
        workspace = qobject_cast<QSplitter *>(m_terminalTabs->currentWidget());
        if (!workspace) {
            return;
        }
    }
    workspace->setOrientation(orientation);
    TerminalSession session = createTerminal(m_terminalTabs->tabText(m_terminalTabs->currentIndex()), workspace);
    if (session.interface) {
        session.interface->showShellInDir(QDir::homePath());
    }
}

bool WaypaneWindow::isDarkTheme() const
{
    if (m_themeMode == ThemeMode::Dark) {
        return true;
    }
    if (m_themeMode == ThemeMode::Light) {
        return false;
    }
    return QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
}

void WaypaneWindow::addConnection()
{
    ProfileDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    Waypane::ConnectionProfile profile = dialog.profile();
    if (profile.id.isEmpty()) {
        profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (!dialog.secret().isEmpty()) {
        QString error;
        if (!Waypane::SecretStore::write(profile.id, dialog.secret(), winId(), &error)) {
            QMessageBox::warning(this, tr("Save secret"), error);
            return;
        }
        profile.secretId = profile.id;
    }
    if (m_connections.upsertConnection(profile)) {
        syncManagedSshConfig();
        refreshConnections(m_connections.rowCount() - 1);
    }
}

void WaypaneWindow::editConnection()
{
    const auto selected = selectedProfile();
    if (!selected) {
        return;
    }
    ProfileDialog dialog(this);
    dialog.setProfile(*selected);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    Waypane::ConnectionProfile profile = dialog.profile();
    if (!dialog.secret().isEmpty()) {
        QString error;
        if (!Waypane::SecretStore::write(profile.id, dialog.secret(), winId(), &error)) {
            QMessageBox::warning(this, tr("Save secret"), error);
            return;
        }
        profile.secretId = profile.id;
    }
    if (m_connections.upsertConnection(profile)) {
        syncManagedSshConfig();
        bool rowOk = false;
        const int row = m_connectionList->currentIndex().data(Qt::UserRole).toInt(&rowOk);
        refreshConnections(rowOk ? row : -1);
    }
}

void WaypaneWindow::deleteConnection()
{
    const auto selected = selectedProfile();
    if (!selected) {
        return;
    }
    if (QMessageBox::question(this, tr("Delete connection"), tr("Delete “%1”? The saved keyring secret will also be removed; private key files are never deleted.").arg(selected->name))
        != QMessageBox::Yes) {
        return;
    }
    if (!selected->secretId.isEmpty()) {
        QString error;
        if (!Waypane::SecretStore::remove(selected->secretId, winId(), &error)) {
            QMessageBox::warning(this, tr("Delete saved secret"), error);
        }
    }
    bool rowOk = false;
    const int row = m_connectionList->currentIndex().data(Qt::UserRole).toInt(&rowOk);
    if (!rowOk || row < 0) {
        return;
    }
    if (m_connections.removeConnection(row)) {
        syncManagedSshConfig();
        refreshConnections(std::min(row, m_connections.rowCount() - 1));
    }
}

void WaypaneWindow::connectSelected()
{
    const auto selected = selectedProfile();
    if (!selected) {
        return;
    }
    if (!openSshSession(*selected)) {
        return;
    }
    m_activeProfileId = selected->id;
    m_workspaceTitle->setText(selected->name);
    m_workspaceEndpoint->setText(QStringLiteral("SSH · %1@%2:%3").arg(selected->username, selected->host, QString::number(selected->port)));
}

void WaypaneWindow::connectSelectedTunnels()
{
    const auto selected = selectedProfile();
    if (!selected) {
        return;
    }
    if (selected->localForwards.isEmpty() && selected->remoteForwards.isEmpty() && selected->dynamicForwards.isEmpty()) {
        QMessageBox::information(this, tr("SSH tunnels"), tr("Add a local, remote, or dynamic forward to this connection first."));
        return;
    }
    if (!openSshSession(*selected, nullptr, true)) {
        return;
    }
    m_activeProfileId = selected->id;
    m_workspaceTitle->setText(tr("%1 tunnels").arg(selected->name));
    m_workspaceEndpoint->setText(tr("TUNNELS · close the terminal tab to stop"));
}

void WaypaneWindow::browseSelectedFiles()
{
    const auto selected = selectedProfile();
    if (!selected) {
        return;
    }
    if (!confirmDiscardRemoteEdit()) {
        return;
    }
    const bool importedAlias = !selected->sshConfigAlias.isEmpty();
    const bool useManagedAlias = !importedAlias && ensureManagedSshConfig();
    m_fileProfile = *selected;
    m_sftpAlias = importedAlias ? selected->sshConfigAlias : (useManagedAlias ? Waypane::ManagedSshConfig::aliasFor(*selected) : QString());
    QUrl sftp;
    sftp.setScheme(QStringLiteral("sftp"));
    sftp.setHost(importedAlias ? selected->sshConfigAlias : (useManagedAlias ? Waypane::ManagedSshConfig::aliasFor(*selected) : selected->host));
    if (!importedAlias && !useManagedAlias) {
        sftp.setPort(selected->port);
        sftp.setUserName(selected->username);
    }
    sftp.setPath(selected->remotePath);
    if (selected->authentication == QStringLiteral("password") && !selected->secretId.isEmpty()) {
        QString error;
        const QString secret = Waypane::SecretStore::read(selected->secretId, winId(), &error);
        if (!secret.isEmpty()) {
            sftp.setPassword(secret);
        } else if (!error.isEmpty()) {
            QMessageBox::warning(this, tr("Open saved password"), error);
        }
    }
    m_fileBrowser->setUrl(sftp, true);
    m_localFileBrowser->setUrl(QUrl::fromLocalFile(QDir::homePath()), true);
    m_localPath->setText(QDir::homePath());
    m_fileStack->show();
    m_fileStack->setCurrentWidget(m_fileBrowserPage);
    m_workspaceSplit->setSizes({std::max(500, m_workspaceSplit->width() / 2), std::max(500, m_workspaceSplit->width() / 2)});
    m_activeProfileId = selected->id;
    m_workspaceTitle->setText(selected->name);
    m_workspaceEndpoint->setText(QStringLiteral("SFTP · %1@%2:%3").arg(selected->username, selected->host, QString::number(selected->port)));
}

void WaypaneWindow::importConnections()
{
    const int imported = m_connections.importOpenSshConfig();
    syncManagedSshConfig();
    refreshConnections();
    if (!m_connections.error().isEmpty()) {
        QMessageBox::warning(this, tr("OpenSSH import"), m_connections.error());
    } else {
        QMessageBox::information(this, tr("OpenSSH import"), tr("Imported or refreshed %1 connection(s).").arg(imported));
    }
}

void WaypaneWindow::uploadFiles()
{
    const KFileItemList items = m_localFileBrowser->selectedItems();
    if (items.isEmpty()) {
        QMessageBox::information(this, tr("Upload"), tr("Select one or more local files first."));
        return;
    }
    QList<QUrl> sources;
    sources.reserve(items.size());
    for (const KFileItem &item : items) {
        sources.append(item.url());
    }
    enqueueTransfer(sources, m_fileBrowser->url(), true);
}

void WaypaneWindow::downloadSelected()
{
    const KFileItemList items = m_fileBrowser->selectedItems();
    if (items.isEmpty()) {
        QMessageBox::information(this, tr("Download"), tr("Select one or more remote files first."));
        return;
    }
    QList<QUrl> sources;
    sources.reserve(items.size());
    for (const KFileItem &item : items) {
        sources.append(item.url());
    }
    const QUrl destination = m_localFileBrowser->url();
    enqueueTransfer(sources, destination, false);
}

void WaypaneWindow::editRemoteSelected()
{
    const KFileItemList items = m_fileBrowser->selectedItems();
    if (items.size() != 1 || items.first().isDir()) {
        QMessageBox::information(this, tr("Remote editor"), tr("Select one remote text file to edit."));
        return;
    }
    openRemoteEditor(items.first().url());
}

void WaypaneWindow::openRemoteEditor(const QUrl &url)
{
    m_transferStatus->setText(tr("Opening remote file…"));
    auto *job = KIO::storedGet(url);
    connect(job, &KJob::result, this, [this, job, url] {
        if (job->error()) {
            m_transferStatus->setText(tr("Could not open remote file"));
            QMessageBox::warning(this, tr("Remote editor"), job->errorString());
            return;
        }
        const QByteArray data = job->data();
        constexpr qsizetype maximumEditorBytes = 5 * 1024 * 1024;
        if (data.size() > maximumEditorBytes || data.contains('\0')) {
            m_transferStatus->setText(tr("Unsupported editor file"));
            QMessageBox::information(this, tr("Remote editor"), tr("The built-in editor supports UTF-8 text files up to 5 MiB."));
            return;
        }
        m_remoteEditorUrl = url;
        m_remoteEditorPath->setText(url.path());
        m_remoteEditor->setPlainText(QString::fromUtf8(data));
        m_remoteEditor->document()->setModified(false);
        m_fileStack->setCurrentWidget(m_remoteEditorPage);
        m_transferStatus->setText(tr("Remote file opened"));
    });
}

void WaypaneWindow::saveRemoteEdit()
{
    if (!m_remoteEditorUrl.isValid()) {
        return;
    }
    m_transferStatus->setText(tr("Saving remote file…"));
    auto *job = KIO::storedPut(m_remoteEditor->toPlainText().toUtf8(), m_remoteEditorUrl, -1, KIO::Overwrite);
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            m_transferStatus->setText(tr("Remote save failed"));
            QMessageBox::warning(this, tr("Remote editor"), job->errorString());
            return;
        }
        m_remoteEditor->document()->setModified(false);
        m_transferStatus->setText(tr("Remote file saved"));
        m_fileBrowser->rereadDir();
    });
}

void WaypaneWindow::closeFilePanel()
{
    if (!confirmDiscardRemoteEdit()) {
        return;
    }
    m_fileBrowser->close();
    m_fileStack->hide();
    m_fileProfile.reset();
    m_sftpAlias.clear();
}

bool WaypaneWindow::confirmDiscardRemoteEdit()
{
    if (!m_remoteEditor || m_fileStack->currentWidget() != m_remoteEditorPage || !m_remoteEditor->document()->isModified()) {
        return true;
    }
    const bool discard = QMessageBox::question(this,
                                               tr("Discard remote changes?"),
                                               tr("The remote file has unsaved changes. Discard them and leave the editor?"),
                                               QMessageBox::Discard | QMessageBox::Cancel,
                                               QMessageBox::Cancel)
        == QMessageBox::Discard;
    if (discard) {
        m_remoteEditor->document()->setModified(false);
    }
    return discard;
}

void WaypaneWindow::enqueueTransfer(const QList<QUrl> &sources, const QUrl &destination, bool upload)
{
    if (!m_fileProfile) {
        return;
    }
    m_transferQueue.enqueue(TransferRequest{sources, destination, *m_fileProfile, m_sftpAlias, upload});
    if (m_transferActive) {
        m_transferStatus->setText(tr("Transferring · %1 queued").arg(m_transferQueue.size()));
        return;
    }
    startNextTransfer();
}

void WaypaneWindow::startNextTransfer()
{
    if (m_transferActive || m_transferQueue.isEmpty()) {
        return;
    }
    m_transferActive = true;
    const TransferRequest request = m_transferQueue.dequeue();
    if (!startRsyncTransfer(request)) {
        copyWithKio(request);
    }
}

void WaypaneWindow::finishTransfer(const QString &status)
{
    m_transferActive = false;
    m_transferStatus->setText(m_transferQueue.isEmpty() ? status : tr("%1 · %2 queued").arg(status).arg(m_transferQueue.size()));
    startNextTransfer();
}

void WaypaneWindow::copyWithKio(const TransferRequest &request)
{
    m_transferStatus->setText(tr("Transferring with SFTP…"));
    auto *job = KIO::copy(request.sources, request.destination);
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            finishTransfer(tr("Transfer failed"));
            QMessageBox::warning(this, tr("File transfer failed"), job->errorString());
        } else {
            finishTransfer(tr("Transfer complete · SFTP"));
            m_localFileBrowser->rereadDir();
            m_fileBrowser->rereadDir();
        }
    });
}

bool WaypaneWindow::startRsyncTransfer(const TransferRequest &request)
{
    const QString rsync = QStandardPaths::findExecutable(QStringLiteral("rsync"));
    if (rsync.isEmpty() || request.sshAlias.isEmpty() || request.profile.authentication == QStringLiteral("password")
        || !request.profile.secretId.isEmpty()) {
        return false;
    }
    const QString ssh = Waypane::SshRuntime::executable();
    if (ssh.contains(QRegularExpression(QStringLiteral("\\s")))) {
        return false;
    }

    QStringList arguments{QStringLiteral("-a"), QStringLiteral("--partial"), QStringLiteral("--append-verify"), QStringLiteral("--protect-args"), QStringLiteral("-e"), ssh};
    if (request.upload) {
        for (const QUrl &source : request.sources) {
            if (!source.isLocalFile()) {
                return false;
            }
            arguments.append(source.toLocalFile());
        }
        QString remotePath = request.destination.path();
        if (!remotePath.endsWith(QLatin1Char('/'))) {
            remotePath += QLatin1Char('/');
        }
        arguments.append(request.sshAlias + QLatin1Char(':') + remotePath);
    } else {
        for (const QUrl &source : request.sources) {
            arguments.append(request.sshAlias + QLatin1Char(':') + source.path());
        }
        QString localPath = request.destination.toLocalFile();
        if (!localPath.endsWith(QLatin1Char('/'))) {
            localPath += QLatin1Char('/');
        }
        arguments.append(localPath);
    }

    auto *process = new QProcess(this);
    process->setProgram(rsync);
    process->setArguments(arguments);
    process->start();
    if (!process->waitForStarted(500)) {
        process->deleteLater();
        return false;
    }
    m_transferStatus->setText(tr("Transferring with rsync…"));
    connect(process, &QProcess::finished, this, [this, process, request](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit && exitCode == 0) {
            finishTransfer(tr("Transfer complete · rsync"));
            m_localFileBrowser->rereadDir();
            m_fileBrowser->rereadDir();
        } else {
            m_transferStatus->setText(tr("Retrying with SFTP…"));
            copyWithKio(request);
        }
        process->deleteLater();
    });
    return true;
}

bool WaypaneWindow::ensureManagedSshConfig()
{
    QString error;
    if (!Waypane::ManagedSshConfig::write(m_connections.profiles(), &error)) {
        QMessageBox::warning(this, tr("SFTP configuration"), tr("Could not write Waypane's SSH configuration: %1").arg(error));
        return false;
    }
    if (Waypane::ManagedSshConfig::isIncluded()) {
        return true;
    }
    if (Waypane::ManagedSshConfig::hasInclude()) {
        if (!Waypane::ManagedSshConfig::installInclude(&error)) {
            QMessageBox::warning(this, tr("SFTP configuration"), tr("Could not repair Waypane's SSH include: %1").arg(error));
            return false;
        }
        return true;
    }

    QMessageBox prompt(QMessageBox::Question,
                       tr("Enable profile-aware SFTP?"),
                       tr("To use this profile's key, jump hosts, and host-key settings in the native SFTP browser, Waypane needs to add one Include line to ~/.ssh/config. Your existing SSH settings will remain unchanged."),
                       QMessageBox::NoButton,
                       this);
    auto *enable = prompt.addButton(tr("Enable SFTP integration"), QMessageBox::AcceptRole);
    prompt.addButton(tr("Terminal only"), QMessageBox::RejectRole);
    prompt.exec();
    if (prompt.clickedButton() != enable) {
        return false;
    }
    if (!Waypane::ManagedSshConfig::installInclude(&error)) {
        QMessageBox::warning(this, tr("SFTP configuration"), tr("Could not update ~/.ssh/config: %1").arg(error));
        return false;
    }
    return true;
}

void WaypaneWindow::syncManagedSshConfig()
{
    if (!Waypane::ManagedSshConfig::isIncluded() && Waypane::ManagedSshConfig::hasInclude()) {
        QString migrationError;
        if (!Waypane::ManagedSshConfig::installInclude(&migrationError)) {
            m_workspaceEndpoint->setText(tr("Could not repair managed SSH include: %1").arg(migrationError));
            return;
        }
    }
    if (!Waypane::ManagedSshConfig::isIncluded()) {
        return;
    }
    QString error;
    if (!Waypane::ManagedSshConfig::write(m_connections.profiles(), &error)) {
        m_workspaceEndpoint->setText(tr("Could not update managed SSH profiles: %1").arg(error));
    }
}

void WaypaneWindow::filterConnections(const QString &text)
{
    const QString needle = text.trimmed();
    const auto filterBranch = [&](auto &&self, QStandardItem *parent, bool ancestorMatches) -> bool {
        bool anyVisible = false;
        for (int childRow = 0; childRow < parent->rowCount(); ++childRow) {
            QStandardItem *item = parent->child(childRow);
            const int flatRow = item->data(Qt::UserRole).toInt();
            bool visible = needle.isEmpty() || ancestorMatches;
            if (flatRow < 0) {
                const bool folderMatches = item->text().contains(needle, Qt::CaseInsensitive);
                visible = self(self, item, visible || folderMatches);
                if (visible && !needle.isEmpty()) {
                    m_connectionList->expand(item->index());
                }
            } else {
                const auto profile = m_connections.profileAt(flatRow);
                visible = visible || (profile && (profile->name.contains(needle, Qt::CaseInsensitive)
                                                   || profile->host.contains(needle, Qt::CaseInsensitive)
                                                   || profile->username.contains(needle, Qt::CaseInsensitive)
                                                   || profile->group.contains(needle, Qt::CaseInsensitive)
                                                   || profile->tags.join(QLatin1Char(' ')).contains(needle, Qt::CaseInsensitive)));
            }
            m_connectionList->setRowHidden(childRow, parent->index(), !visible);
            anyVisible = anyVisible || visible;
        }
        return anyVisible;
    };
    filterBranch(filterBranch, m_connectionTreeModel->invisibleRootItem(), false);
}

void WaypaneWindow::updateSelection()
{
    const auto selected = selectedProfile();
    const bool available = selected.has_value();
    m_connectButton->setEnabled(available);
    m_sftpButton->setEnabled(available);
    m_editButton->setEnabled(available);
    m_deleteButton->setEnabled(available);
    if (selected) {
        m_workspaceTitle->setText(selected->name);
        const QString user = selected->username.isEmpty() ? QString() : selected->username + QLatin1Char('@');
        m_workspaceEndpoint->setText(user + selected->host + QLatin1Char(':') + QString::number(selected->port));
    }
}

void WaypaneWindow::cycleTheme()
{
    m_themeMode = static_cast<ThemeMode>((static_cast<int>(m_themeMode) + 1) % 3);
    QSettings().setValue(QStringLiteral("appearance/theme"), static_cast<int>(m_themeMode));
    applyTheme();
}

void WaypaneWindow::openSettings()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        ensureTerminalThemes();
        applyTerminalThemes();
    }
}

void WaypaneWindow::configurePortForwards()
{
    const auto selected = selectedProfile();
    if (!selected) {
        return;
    }
    PortForwardDialog dialog(*selected, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const Waypane::ConnectionProfile profile = dialog.profile();
    if (!m_connections.upsertConnection(profile)) {
        QMessageBox::warning(this, tr("Port forwarding"), m_connections.error());
        return;
    }
    syncManagedSshConfig();
    int row = -1;
    for (int candidate = 0; candidate < m_connections.rowCount(); ++candidate) {
        const auto item = m_connections.profileAt(candidate);
        if (item && item->id == profile.id) {
            row = candidate;
            break;
        }
    }
    refreshConnections(row);
    if (!dialog.startRequested()) {
        return;
    }
    if (profile.localForwards.isEmpty() && profile.remoteForwards.isEmpty() && profile.dynamicForwards.isEmpty()) {
        QMessageBox::information(this, tr("Port forwarding"), tr("Add at least one forward before starting a tunnel session."));
        return;
    }
    if (openSshSession(profile, nullptr, true)) {
        m_activeProfileId = profile.id;
        m_workspaceTitle->setText(tr("%1 tunnels").arg(profile.name));
        m_workspaceEndpoint->setText(tr("TUNNELS · close the terminal tab to stop"));
    }
}

void WaypaneWindow::newLocalTerminal()
{
    TerminalSession session = createTerminal(tr("Local"));
    if (session.interface) {
        session.interface->showShellInDir(QDir::homePath());
        m_activeProfileId.clear();
        m_workspaceTitle->setText(tr("Local workspace"));
        m_workspaceEndpoint->setText(tr("LOCAL · %1").arg(QDir::homePath()));
    }
}

void WaypaneWindow::saveWorkspace() const
{
    if (!m_terminalTabs) {
        return;
    }
    QVariantList tabs;
    for (int tabIndex = 0; tabIndex < m_terminalTabs->count(); ++tabIndex) {
        auto *workspace = qobject_cast<QSplitter *>(m_terminalTabs->widget(tabIndex));
        if (!workspace) {
            continue;
        }
        QStringList panes;
        for (int paneIndex = 0; paneIndex < workspace->count(); ++paneIndex) {
            const QString profileId = m_terminalSessions.value(workspace->widget(paneIndex)).profileId;
            if (profileId != QStringLiteral("__transient__")) {
                panes.append(profileId);
            }
        }
        if (panes.isEmpty()) {
            continue;
        }
        QVariantMap tab;
        tab.insert(QStringLiteral("title"), m_terminalTabs->tabText(tabIndex));
        tab.insert(QStringLiteral("orientation"), static_cast<int>(workspace->orientation()));
        tab.insert(QStringLiteral("panes"), panes);
        tabs.append(tab);
    }
    QSettings settings;
    settings.setValue(QStringLiteral("workspace/tabs"), tabs);
    settings.setValue(QStringLiteral("workspace/windowGeometry"), saveGeometry());
    if (const auto profile = selectedProfile()) {
        settings.setValue(QStringLiteral("workspace/selectedProfileId"), profile->id);
    }
}

void WaypaneWindow::restoreWorkspace()
{
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("workspace/windowGeometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    if (!settings.value(QStringLiteral("workspace/restore"), true).toBool()) {
        return;
    }
    const bool reconnectSsh = settings.value(QStringLiteral("workspace/reconnectSsh"), false).toBool();
    const QVariantList tabs = settings.value(QStringLiteral("workspace/tabs")).toList();
    for (const QVariant &tabValue : tabs) {
        const QVariantMap tab = tabValue.toMap();
        const QString title = tab.value(QStringLiteral("title"), tr("Local")).toString();
        const QStringList panes = tab.value(QStringLiteral("panes")).toStringList();
        QSplitter *workspace = nullptr;
        for (const QString &profileId : panes) {
            bool opened = false;
            if (profileId.isEmpty()) {
                TerminalSession session = createTerminal(title, workspace);
                if (session.interface) {
                    session.interface->showShellInDir(QDir::homePath());
                    opened = true;
                }
            } else if (reconnectSsh) {
                for (const Waypane::ConnectionProfile &profile : m_connections.profiles()) {
                    if (profile.id == profileId) {
                        opened = openSshSession(profile, workspace);
                        break;
                    }
                }
            }
            if (opened && !workspace) {
                workspace = qobject_cast<QSplitter *>(m_terminalTabs->currentWidget());
            }
        }
        if (workspace) {
            workspace->setOrientation(static_cast<Qt::Orientation>(tab.value(QStringLiteral("orientation"), static_cast<int>(Qt::Horizontal)).toInt()));
        }
    }
}

void WaypaneWindow::closeEvent(QCloseEvent *event)
{
    saveWorkspace();
    QWidget::closeEvent(event);
}

bool WaypaneWindow::eventFilter(QObject *watched, QEvent *event)
{
    const bool ontoLocal = m_localFileBrowser && watched == m_localFileBrowser->view()->viewport();
    const bool ontoRemote = m_fileBrowser && watched == m_fileBrowser->view()->viewport();
    if ((!ontoLocal && !ontoRemote)
        || (event->type() != QEvent::DragEnter && event->type() != QEvent::DragMove && event->type() != QEvent::Drop)) {
        return QWidget::eventFilter(watched, event);
    }

    auto *drop = static_cast<QDropEvent *>(event);
    if (!drop->mimeData()->hasUrls()) {
        return QWidget::eventFilter(watched, event);
    }
    const QList<QUrl> urls = drop->mimeData()->urls();
    const bool validCrossPaneDrop = !urls.isEmpty()
        && std::all_of(urls.cbegin(), urls.cend(), [ontoRemote](const QUrl &url) {
               return ontoRemote ? url.isLocalFile() : (!url.isLocalFile() && url.scheme() == QStringLiteral("sftp"));
           });
    if (!validCrossPaneDrop) {
        return QWidget::eventFilter(watched, event);
    }
    drop->acceptProposedAction();
    if (event->type() == QEvent::Drop) {
        enqueueTransfer(urls, ontoRemote ? m_fileBrowser->url() : m_localFileBrowser->url(), ontoRemote);
    }
    return true;
}

void WaypaneWindow::closeTerminal(int index)
{
    QWidget *workspace = m_terminalTabs->widget(index);
    if (!workspace) {
        return;
    }
    QList<QWidget *> terminalWidgets;
    const QList<QWidget *> liveDescendants = workspace->findChildren<QWidget *>();
    for (QWidget *terminalWidget : m_terminalSessions.keys()) {
        if (terminalWidget == workspace || liveDescendants.contains(terminalWidget)) {
            terminalWidgets.append(terminalWidget);
        }
    }
    m_terminalTabs->removeTab(index);
    for (QWidget *terminalWidget : terminalWidgets) {
        const TerminalSession session = m_terminalSessions.take(terminalWidget);
        if (session.part) {
            session.part->deleteLater();
        }
    }
    workspace->deleteLater();
    if (m_terminalTabs->count() == 0) {
        m_terminalStack->setCurrentWidget(m_landingPage);
        m_activeProfileId.clear();
        m_workspaceTitle->setText(tr("Remote workspace"));
        m_workspaceEndpoint->setText(tr("Choose a connection or start a local terminal"));
    }
}

void WaypaneWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateResizeHandles();
}

void WaypaneWindow::updateResizeHandles()
{
    if (m_resizeHandles.size() != 8) {
        return;
    }
    const int edge = 6;
    const int corner = 10;
    const int w = width();
    const int h = height();
    m_resizeHandles[0]->setGeometry(0, corner, edge, h - corner * 2);
    m_resizeHandles[1]->setGeometry(w - edge, corner, edge, h - corner * 2);
    m_resizeHandles[2]->setGeometry(corner, 0, w - corner * 2, edge);
    m_resizeHandles[3]->setGeometry(corner, h - edge, w - corner * 2, edge);
    m_resizeHandles[4]->setGeometry(0, 0, corner, corner);
    m_resizeHandles[5]->setGeometry(w - corner, 0, corner, corner);
    m_resizeHandles[6]->setGeometry(0, h - corner, corner, corner);
    m_resizeHandles[7]->setGeometry(w - corner, h - corner, corner, corner);
    for (ResizeHandle *handle : m_resizeHandles) {
        handle->raise();
        handle->setVisible(!isMaximized());
    }
}
