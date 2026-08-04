// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/waypanewindow.h"
#include "app/profiledialog.h"
#include "app/settingsdialog.h"
#include "waypane/version.h"

#include <KParts/ReadOnlyPart>
#include <kde_terminal_interface.h>

#include <QCoreApplication>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QEvent>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMetaObject>
#include <QSettings>
#include <QPushButton>
#include <QTest>

#include <utility>

class UiLifecycleTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void reportsCompatibilityVersions();
    void closesTabAfterTerminalWidgetWasDestroyed();
    void repeatedlyClosesLiveTerminalTabs();
    void appliesManagedTerminalColorProfile();
    void controlWClosesCurrentTab();
    void insertClipboardShortcutsStayAvailable();
    void terminalExitClosesOwningTab();
    void copiesFlatpakMcpSetupCommands();
    void settingsShowApplicationVersion();
    void addsSavedConnectionAsJumpHost();
};

void UiLifecycleTests::init()
{
    QSettings settings;
    settings.clear();
    settings.setValue(QStringLiteral("workspace/restore"), false);
}

void UiLifecycleTests::reportsCompatibilityVersions()
{
    WaypaneWindow window;
    const QJsonObject status = window.runtimeStatus();
    QCOMPARE(status.value(QStringLiteral("applicationVersion")).toString(), QCoreApplication::applicationVersion());
    QCOMPARE(status.value(QStringLiteral("profileSchemaVersion")).toInt(), 1);
    QCOMPARE(status.value(QStringLiteral("mcpProtocolVersion")).toInt(), 1);
}

void UiLifecycleTests::closesTabAfterTerminalWidgetWasDestroyed()
{
    WaypaneWindow window;
    window.show();
    window.openLocalTerminal();
    QTest::qWait(100);

    const QList<KParts::ReadOnlyPart *> parts = window.findChildren<KParts::ReadOnlyPart *>();
    QVERIFY(!parts.isEmpty());
    QWidget *terminalWidget = parts.first()->widget();
    QVERIFY(terminalWidget);

    terminalWidget->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    QVERIFY(QMetaObject::invokeMethod(&window, "closeTerminal", Qt::DirectConnection, Q_ARG(int, 0)));
    QCoreApplication::processEvents();
    const QJsonObject status = window.runtimeStatus();
    QCOMPARE(status.value(QStringLiteral("terminalTabCount")).toInt(), 0);
    QCOMPARE(status.value(QStringLiteral("terminalCount")).toInt(), 0);
}

void UiLifecycleTests::repeatedlyClosesLiveTerminalTabs()
{
    WaypaneWindow window;
    window.show();
    QTest::qWait(100);

    for (int iteration = 0; iteration < 5; ++iteration) {
        window.openLocalTerminal();
        QVERIFY(QMetaObject::invokeMethod(&window, "closeTerminal", Qt::DirectConnection, Q_ARG(int, 0)));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();

        const QJsonObject status = window.runtimeStatus();
        QCOMPARE(status.value(QStringLiteral("terminalTabCount")).toInt(), 0);
        QCOMPARE(status.value(QStringLiteral("terminalCount")).toInt(), 0);
    }
}

void UiLifecycleTests::appliesManagedTerminalColorProfile()
{
    QSettings().setValue(QStringLiteral("terminal/theme"), QStringLiteral("day"));
    WaypaneWindow window;
    window.openLocalTerminal();

    const QList<KParts::ReadOnlyPart *> parts = window.findChildren<KParts::ReadOnlyPart *>();
    QVERIFY(!parts.isEmpty());
    auto *terminal = qobject_cast<TerminalInterface *>(parts.first());
    QVERIFY(terminal);
    QVERIFY(terminal->availableProfiles().contains(QStringLiteral("Waypane Day")));
    QCOMPARE(terminal->currentProfileName(), QStringLiteral("Waypane Day"));
    QSettings().remove(QStringLiteral("terminal/theme"));
}

void UiLifecycleTests::controlWClosesCurrentTab()
{
    WaypaneWindow window;
    window.show();
    window.openLocalTerminal();
    QTest::qWait(100);

    QTest::keyClick(&window, Qt::Key_W, Qt::ControlModifier);
    QTRY_COMPARE_WITH_TIMEOUT(window.runtimeStatus().value(QStringLiteral("terminalTabCount")).toInt(), 0, 2000);
}

void UiLifecycleTests::insertClipboardShortcutsStayAvailable()
{
    WaypaneWindow window;
    window.show();
    window.openLocalTerminal();
    QTest::qWait(150);

    const QList<KParts::ReadOnlyPart *> parts = window.findChildren<KParts::ReadOnlyPart *>();
    QVERIFY(!parts.isEmpty());
    QWidget *terminalWidget = parts.first()->widget();
    QVERIFY(terminalWidget);

    QWidget *display = nullptr;
    QList<QWidget *> candidates = terminalWidget->findChildren<QWidget *>();
    candidates.prepend(terminalWidget);
    for (QWidget *candidate : std::as_const(candidates)) {
        if (QByteArray(candidate->metaObject()->className()).contains("TerminalDisplay")) {
            display = candidate;
            break;
        }
    }
    QVERIFY(display);

    // KonsolePart associates its session actions with the display, so the
    // Insert-based clipboard shortcuts must be registered there.
    bool copyShortcutAvailable = false;
    bool pasteShortcutAvailable = false;
    const QList<QAction *> actions = display->actions();
    for (const QAction *action : actions) {
        copyShortcutAvailable = copyShortcutAvailable || action->shortcuts().contains(QKeySequence(Qt::CTRL | Qt::Key_Insert));
        pasteShortcutAvailable = pasteShortcutAvailable || action->shortcuts().contains(QKeySequence(Qt::SHIFT | Qt::Key_Insert));
    }
    QVERIFY(copyShortcutAvailable);
    QVERIFY(pasteShortcutAvailable);

    // The part must not swallow the combinations before Qt's shortcut system
    // can trigger those actions; an accepted ShortcutOverride event means the
    // key would be sent to the terminal instead.
    QKeyEvent copyOverride(QEvent::ShortcutOverride, Qt::Key_Insert, Qt::ControlModifier);
    copyOverride.ignore();
    QApplication::sendEvent(display, &copyOverride);
    QVERIFY(!copyOverride.isAccepted());

    QKeyEvent pasteOverride(QEvent::ShortcutOverride, Qt::Key_Insert, Qt::ShiftModifier);
    pasteOverride.ignore();
    QApplication::sendEvent(display, &pasteOverride);
    QVERIFY(!pasteOverride.isAccepted());
}

void UiLifecycleTests::terminalExitClosesOwningTab()
{
    WaypaneWindow window;
    window.show();
    window.openLocalTerminal();
    QTest::qWait(150);

    const QList<KParts::ReadOnlyPart *> parts = window.findChildren<KParts::ReadOnlyPart *>();
    QVERIFY(!parts.isEmpty());
    QWidget *terminalWidget = parts.first()->widget();
    QVERIFY(terminalWidget);
    auto *terminal = qobject_cast<TerminalInterface *>(parts.first());
    QVERIFY(terminal);
    terminal->sendInput(QString(QChar(0x04)));
    QTRY_COMPARE_WITH_TIMEOUT(window.runtimeStatus().value(QStringLiteral("terminalTabCount")).toInt(), 0, 3000);
}

void UiLifecycleTests::copiesFlatpakMcpSetupCommands()
{
    qputenv("FLATPAK_ID", QByteArrayLiteral("dev.tuska.waypane"));
    SettingsDialog dialog;

    auto *codex = dialog.findChild<QPushButton *>(QStringLiteral("copyCodexMcpButton"));
    auto *claude = dialog.findChild<QPushButton *>(QStringLiteral("copyClaudeMcpButton"));
    auto *json = dialog.findChild<QPushButton *>(QStringLiteral("copyMcpJsonButton"));
    QVERIFY(codex);
    QVERIFY(claude);
    QVERIFY(json);

    codex->click();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("codex mcp add waypane -- flatpak run --command=waypane-mcp dev.tuska.waypane"));
    claude->click();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("claude mcp add --transport stdio --scope user waypane -- flatpak run --command=waypane-mcp dev.tuska.waypane"));
    json->click();
    QVERIFY(QApplication::clipboard()->text().contains(QStringLiteral("\"command\": \"flatpak\"")));
    QVERIFY(QApplication::clipboard()->text().contains(QStringLiteral("dev.tuska.waypane")));
    qunsetenv("FLATPAK_ID");
}

void UiLifecycleTests::settingsShowApplicationVersion()
{
    SettingsDialog dialog;
    auto *version = dialog.findChild<QLabel *>(QStringLiteral("versionLabel"));
    QVERIFY(version);
    QCOMPARE(version->text(), QStringLiteral("Version %1").arg(QStringLiteral(WAYPANE_VERSION)));
}

void UiLifecycleTests::addsSavedConnectionAsJumpHost()
{
    Waypane::ConnectionProfile jumpHost;
    jumpHost.id = QStringLiteral("jump-host-id");
    jumpHost.name = QStringLiteral("Production bastion");
    jumpHost.host = QStringLiteral("bastion.example.test");
    jumpHost.username = QStringLiteral("operator");
    jumpHost.port = 2222;

    ProfileDialog dialog;
    dialog.setAvailableJumpHosts({jumpHost});
    auto *picker = dialog.findChild<QComboBox *>(QStringLiteral("savedJumpHostPicker"));
    auto *add = dialog.findChild<QPushButton *>(QStringLiteral("addSavedJumpHostButton"));
    QVERIFY(picker);
    QVERIFY(add);
    QCOMPARE(picker->count(), 2);
    QVERIFY(picker->itemText(1).contains(QStringLiteral("Production bastion")));
    QVERIFY(picker->itemText(1).contains(QStringLiteral("operator@bastion.example.test:2222")));

    picker->setCurrentIndex(1);
    add->click();
    QCOMPARE(dialog.profile().jumpHosts, QStringList({QStringLiteral("waypane-jump-host-id")}));

    picker->setCurrentIndex(1);
    add->click();
    QCOMPARE(dialog.profile().jumpHosts, QStringList({QStringLiteral("waypane-jump-host-id")}));
}

QTEST_MAIN(UiLifecycleTests)
#include "ui_lifecycle_tests.moc"
