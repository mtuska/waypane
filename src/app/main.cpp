// SPDX-License-Identifier: GPL-3.0-or-later
#include "waypanewindow.h"
#include "livecontrolserver.h"
#include "waypane/version.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QHash>
#include <QIcon>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Tuska"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("tuska.dev"));
    QCoreApplication::setApplicationName(QStringLiteral("waypane"));
    QCoreApplication::setApplicationVersion(QStringLiteral(WAYPANE_VERSION));
    application.setDesktopFileName(QStringLiteral("dev.tuska.waypane"));
    application.setWindowIcon(QIcon(QStringLiteral(":/assets/icons/waypane.png")));

    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Native terminal, SSH, and SFTP workspace"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption themeOption(QStringLiteral("theme"), QStringLiteral("Override appearance: system, light, or dark"), QStringLiteral("mode"));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"), QStringLiteral("Save a UI screenshot and exit (development/testing)"), QStringLiteral("path"));
    const QCommandLineOption screenshotDelayOption(QStringLiteral("screenshot-delay"), QStringLiteral("Wait this many milliseconds before capturing a development screenshot"), QStringLiteral("milliseconds"), QStringLiteral("1200"));
    const QCommandLineOption connectOption(QStringLiteral("connect"), QStringLiteral("Connect to a saved profile ID"), QStringLiteral("profile-id"));
    const QCommandLineOption browseSftpOption(QStringLiteral("browse-sftp"), QStringLiteral("Open SFTP for a saved profile ID without opening SSH"), QStringLiteral("profile-id"));
    const QCommandLineOption splitOption(QStringLiteral("split"), QStringLiteral("Split the current terminal right or down (development/testing)"), QStringLiteral("direction"));
    const QCommandLineOption editRemoteOption(QStringLiteral("edit-remote"), QStringLiteral("Open profile-id:/absolute/path in the remote editor (development/testing)"), QStringLiteral("profile-id:path"));
    parser.addOptions({themeOption, screenshotOption, screenshotDelayOption, connectOption, browseSftpOption, splitOption, editRemoteOption});
    parser.process(application);
    if (parser.isSet(themeOption)) {
        const QString mode = parser.value(themeOption).toLower();
        const QHash<QString, int> modes{{QStringLiteral("system"), 0}, {QStringLiteral("light"), 1}, {QStringLiteral("dark"), 2}};
        if (!modes.contains(mode)) {
            parser.showHelp(2);
        }
        QSettings().setValue(QStringLiteral("appearance/theme"), modes.value(mode));
    }

    WaypaneWindow window;
    LiveControlServer controlServer(&window);
    QString controlError;
    if (!controlServer.start(&controlError)) {
        qWarning("Waypane live control unavailable: %s", qPrintable(controlError));
    }
    window.show();
    if (parser.isSet(connectOption)) {
        const QString profileId = parser.value(connectOption);
        QTimer::singleShot(100, &window, [&window, profileId] {
            window.connectToProfileId(profileId);
        });
    }
    if (parser.isSet(browseSftpOption)) {
        const QString profileId = parser.value(browseSftpOption);
        QTimer::singleShot(100, &window, [&window, profileId] {
            window.browseProfileFilesById(profileId);
        });
    }
    if (parser.isSet(splitOption)) {
        const QString direction = parser.value(splitOption).toLower();
        if (direction != QStringLiteral("right") && direction != QStringLiteral("down")) {
            parser.showHelp(2);
        }
        QTimer::singleShot(200, &window, [&window, direction] {
            window.splitLocalTerminal(direction == QStringLiteral("down") ? Qt::Vertical : Qt::Horizontal);
        });
    }
    if (parser.isSet(editRemoteOption)) {
        const QString target = parser.value(editRemoteOption);
        const qsizetype separator = target.indexOf(QLatin1Char(':'));
        if (separator <= 0 || separator + 1 >= target.size()) {
            parser.showHelp(2);
        }
        const QString profileId = target.left(separator);
        const QString remotePath = target.mid(separator + 1);
        QTimer::singleShot(200, &window, [&window, profileId, remotePath] {
            window.editRemoteFileById(profileId, remotePath);
        });
    }
    if (parser.isSet(screenshotOption)) {
        const QString path = parser.value(screenshotOption);
        bool validDelay = false;
        const int requestedDelay = parser.value(screenshotDelayOption).toInt(&validDelay);
        const int delay = validDelay ? qBound(100, requestedDelay, 30000) : 1200;
        QTimer::singleShot(delay, &application, [&application, &window, path] {
            application.exit(window.grab().save(path) ? 0 : 3);
        });
    }
    return application.exec();
}
