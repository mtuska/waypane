// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/connectionprofile.h"
#include "core/connectionstore.h"
#include "core/managedsshconfig.h"
#include "core/opensshconfigimporter.h"
#include "core/sshcommandbuilder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class CoreTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsArgumentVectorWithoutShellInterpolation();
    void roundTripsProfiles();
    void validatesRequiredFields();
    void importsOpenSshProfiles();
    void writesManagedOpenSshConfig();
    void repairsIncludeAfterHostBlock();
    void scopesLegacyAlgorithmsToOneProfile();
    void buildsTunnelOnlyArgumentVector();
    void usesManagedConfigForSavedJumpHosts();
};

void CoreTests::buildsArgumentVectorWithoutShellInterpolation()
{
    Waypane::ConnectionProfile profile;
    profile.name = QStringLiteral("Production");
    profile.host = QStringLiteral("db.example.test");
    profile.username = QStringLiteral("deploy");
    profile.port = 2222;
    profile.identityFile = QStringLiteral("/home/me/Keys/production key");
    profile.jumpHosts = {QStringLiteral("edge"), QStringLiteral("bastion")};
    profile.agentForwarding = true;

    QCOMPARE(Waypane::SshCommandBuilder::arguments(profile),
             QStringList({QStringLiteral("-p"),
                          QStringLiteral("2222"),
                          QStringLiteral("-i"),
                          QStringLiteral("/home/me/Keys/production key"),
                          QStringLiteral("-J"),
                          QStringLiteral("edge,bastion"),
                          QStringLiteral("-A"),
                          QStringLiteral("deploy@db.example.test")}));
}

void CoreTests::roundTripsProfiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Waypane::ConnectionStore store(directory.filePath(QStringLiteral("connections.json")));

    Waypane::ConnectionProfile profile;
    profile.id = QStringLiteral("example-id");
    profile.name = QStringLiteral("Example");
    profile.host = QStringLiteral("example.test");
    profile.tags = {QStringLiteral("prod"), QStringLiteral("database")};
    profile.localForwards = {QStringLiteral("127.0.0.1:5432:db:5432")};
    profile.secretId = QStringLiteral("keyring-item-42");

    QString error;
    QVERIFY2(store.save({profile}, &error), qPrintable(error));
    const QList<Waypane::ConnectionProfile> loaded = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().id, profile.id);
    QCOMPARE(loaded.first().tags, profile.tags);
    QCOMPARE(loaded.first().localForwards, profile.localForwards);
    QCOMPARE(loaded.first().secretId, profile.secretId);
}

void CoreTests::validatesRequiredFields()
{
    Waypane::ConnectionProfile profile;
    QString error;
    QVERIFY(!profile.isValid(&error));
    QCOMPARE(error, QStringLiteral("Connection name is required"));
    profile.name = QStringLiteral("Example");
    QVERIFY(!profile.isValid(&error));
    QCOMPARE(error, QStringLiteral("Host is required"));
}

void CoreTests::importsOpenSshProfiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile config(directory.filePath(QStringLiteral("config")));
    QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Text));
    config.write("Host production-api\n"
                 "  HostName api.internal.example\n"
                 "  User deploy\n"
                 "  Port 2222\n"
                 "  IdentityFile /home/me/keys/production key\n"
                 "  ProxyJump edge,bastion\n"
                 "  LocalForward 127.0.0.1:5432 db.internal:5432\n"
                 "  DynamicForward 127.0.0.1:1080\n"
                 "  ForwardAgent yes\n"
                 "Host *.internal\n"
                 "  User ignored\n");
    config.close();

    QString error;
    const QList<Waypane::ConnectionProfile> profiles = Waypane::OpenSshConfigImporter::importFile(config.fileName(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(profiles.size(), 1);
    QCOMPARE(profiles.first().name, QStringLiteral("production-api"));
    QCOMPARE(profiles.first().sshConfigAlias, QStringLiteral("production-api"));
    QCOMPARE(profiles.first().host, QStringLiteral("api.internal.example"));
    QCOMPARE(profiles.first().port, static_cast<quint16>(2222));
    QCOMPARE(profiles.first().jumpHosts, QStringList({QStringLiteral("edge"), QStringLiteral("bastion")}));
    QCOMPARE(profiles.first().localForwards, QStringList({QStringLiteral("127.0.0.1:5432 db.internal:5432")}));
    QCOMPARE(profiles.first().dynamicForwards, QStringList({QStringLiteral("127.0.0.1:1080")}));
    QVERIFY(profiles.first().agentForwarding);
}

void CoreTests::writesManagedOpenSshConfig()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Waypane::ConnectionProfile profile;
    profile.id = QStringLiteral("e2e-id");
    profile.name = QStringLiteral("E2E");
    profile.host = QStringLiteral("127.0.0.1");
    profile.port = 40222;
    profile.username = QStringLiteral("tester");
    profile.identityFile = QStringLiteral("/tmp/key with spaces");
    profile.jumpHosts = {QStringLiteral("edge"), QStringLiteral("bastion")};
    profile.hostKeyPolicy = QStringLiteral("accept-new");

    const QString managed = directory.filePath(QStringLiteral("waypane/config"));
    const QString user = directory.filePath(QStringLiteral("ssh/config"));
    QString error;
    QVERIFY2(Waypane::ManagedSshConfig::write({profile}, &error, managed), qPrintable(error));
    QVERIFY2(Waypane::ManagedSshConfig::installInclude(&error, user, managed), qPrintable(error));
    QVERIFY(Waypane::ManagedSshConfig::isIncluded(user, managed));
    QVERIFY(Waypane::ManagedSshConfig::installInclude(&error, user, managed));

    QFile file(managed);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString generated = QString::fromUtf8(file.readAll());
    QVERIFY(generated.contains(QStringLiteral("Host waypane-e2e-id")));
    QVERIFY(generated.contains(QStringLiteral("IdentityFile \"/tmp/key with spaces\"")));
    QVERIFY(generated.contains(QStringLiteral("ProxyJump edge,bastion")));

    QFile includeFile(user);
    QVERIFY(includeFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(includeFile.readAll()).count(QStringLiteral("Include")), 1);
}

void CoreTests::repairsIncludeAfterHostBlock()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Waypane::ConnectionProfile profile;
    profile.id = QStringLiteral("repair-test");
    profile.name = QStringLiteral("Repair test");
    profile.host = QStringLiteral("sftp.example.test");
    profile.port = 60081;
    profile.username = QStringLiteral("deploy");

    const QString managed = directory.filePath(QStringLiteral("waypane/config"));
    const QString user = directory.filePath(QStringLiteral("ssh/config"));
    QString error;
    QVERIFY2(Waypane::ManagedSshConfig::write({profile}, &error, managed), qPrintable(error));
    QDir().mkpath(QFileInfo(user).absolutePath());
    QFile config(user);
    QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Text));
    config.write("Host existing\n"
                 "    HostName existing.example.test\n\n"
                 "# Waypane managed connection profiles\n"
                 "Include \"");
    config.write(managed.toUtf8());
    config.write("\"\n");
    config.close();

    QVERIFY(Waypane::ManagedSshConfig::hasInclude(user, managed));
    QVERIFY(!Waypane::ManagedSshConfig::isIncluded(user, managed));
    QVERIFY2(Waypane::ManagedSshConfig::installInclude(&error, user, managed), qPrintable(error));
    QVERIFY(Waypane::ManagedSshConfig::isIncluded(user, managed));

    QVERIFY(config.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString repaired = QString::fromUtf8(config.readAll());
    QVERIFY(repaired.startsWith(QStringLiteral("# Waypane managed connection profiles\nInclude \"")));
    QCOMPARE(repaired.count(managed), 1);

    const QString ssh = QStandardPaths::findExecutable(QStringLiteral("ssh"));
    if (ssh.isEmpty()) {
        QSKIP("OpenSSH client is unavailable");
    }
    QProcess process;
    process.start(ssh, {QStringLiteral("-G"), QStringLiteral("-F"), user, Waypane::ManagedSshConfig::aliasFor(profile)});
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitCode(), 0);
    const QString resolvedConfig = QString::fromUtf8(process.readAllStandardOutput());
    QVERIFY(resolvedConfig.contains(QStringLiteral("hostname sftp.example.test\n")));
    QVERIFY(resolvedConfig.contains(QStringLiteral("port 60081\n")));
}

void CoreTests::scopesLegacyAlgorithmsToOneProfile()
{
    Waypane::ConnectionProfile modern;
    modern.name = QStringLiteral("Modern");
    modern.host = QStringLiteral("modern.example.test");
    QVERIFY(!Waypane::SshCommandBuilder::arguments(modern).join(QLatin1Char(' ')).contains(QStringLiteral("ssh-rsa")));

    Waypane::ConnectionProfile legacy = modern;
    legacy.name = QStringLiteral("Legacy appliance");
    legacy.host = QStringLiteral("legacy.example.test");
    legacy.legacyCompatibility = true;
    const QStringList arguments = Waypane::SshCommandBuilder::arguments(legacy);
    QVERIFY(arguments.contains(QStringLiteral("HostKeyAlgorithms=+ssh-rsa")));
    QVERIFY(arguments.contains(QStringLiteral("KexAlgorithms=+diffie-hellman-group1-sha1,diffie-hellman-group14-sha1")));
}

void CoreTests::buildsTunnelOnlyArgumentVector()
{
    Waypane::ConnectionProfile profile;
    profile.name = QStringLiteral("Tunnel host");
    profile.host = QStringLiteral("tunnel.example.test");
    profile.username = QStringLiteral("deploy");
    profile.localForwards = {QStringLiteral("127.0.0.1:8080:web:80")};
    QCOMPARE(Waypane::SshCommandBuilder::arguments(profile, true),
             QStringList({QStringLiteral("-L"),
                          QStringLiteral("127.0.0.1:8080:web:80"),
                          QStringLiteral("-N"),
                          QStringLiteral("-T"),
                          QStringLiteral("deploy@tunnel.example.test")}));

    profile.sshConfigAlias = QStringLiteral("saved-alias");
    QCOMPARE(Waypane::SshCommandBuilder::arguments(profile, true),
             QStringList({QStringLiteral("-L"),
                          QStringLiteral("127.0.0.1:8080:web:80"),
                          QStringLiteral("-N"),
                          QStringLiteral("-T"),
                          QStringLiteral("deploy@tunnel.example.test")}));
}

void CoreTests::usesManagedConfigForSavedJumpHosts()
{
    Waypane::ConnectionProfile profile;
    profile.name = QStringLiteral("Internal database");
    profile.host = QStringLiteral("db.internal.example");
    profile.jumpHosts = {QStringLiteral("waypane-bastion-id")};

    QCOMPARE(Waypane::SshCommandBuilder::arguments(profile, false, QStringLiteral("/tmp/waypane-managed-config")),
             QStringList({QStringLiteral("-F"),
                          QStringLiteral("/tmp/waypane-managed-config"),
                          QStringLiteral("-J"),
                          QStringLiteral("waypane-bastion-id"),
                          QStringLiteral("db.internal.example")}));

    profile.sshConfigAlias = QStringLiteral("imported-database");
    QCOMPARE(Waypane::SshCommandBuilder::arguments(profile, false, QStringLiteral("/tmp/waypane-managed-config")),
             QStringList({QStringLiteral("-F"),
                          QStringLiteral("/tmp/waypane-managed-config"),
                          QStringLiteral("imported-database")}));
}

QTEST_MAIN(CoreTests)
#include "core_tests.moc"
