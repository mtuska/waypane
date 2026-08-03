// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/waypanewindow.h"
#include "app/settingsdialog.h"
#include "waypane/version.h"

#include <KParts/ReadOnlyPart>
#include <kde_terminal_interface.h>

#include <QCoreApplication>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QJsonObject>
#include <QLabel>
#include <QMetaObject>
#include <QSettings>
#include <QPushButton>
#include <QTest>

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
    void terminalExitClosesOwningTab();
    void copiesFlatpakMcpSetupCommands();
    void settingsShowApplicationVersion();
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

QTEST_MAIN(UiLifecycleTests)
#include "ui_lifecycle_tests.moc"
