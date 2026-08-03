// SPDX-License-Identifier: GPL-3.0-or-later
#include "profiledialog.h"

#include "core/sshruntime.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSysInfo>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

ProfileDialog::ProfileDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    setMinimumWidth(520);
    setMinimumHeight(640);
    resize(560, 720);
    setObjectName(QStringLiteral("profileDialog"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);
    outer->setSpacing(0);

    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("dialogFrame"));
    outer->addWidget(frame);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 0, 20, 20);
    layout->setSpacing(14);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel(tr("Connection profile"), frame);
    title->setObjectName(QStringLiteral("dialogTitle"));
    auto *close = new QToolButton(frame);
    close->setObjectName(QStringLiteral("windowCloseButton"));
    close->setText(QStringLiteral("×"));
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(close);
    layout->addLayout(titleRow);

    auto *formWidget = new QWidget(frame);
    auto *form = new QFormLayout(formWidget);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_name = new QLineEdit(frame);
    m_name->setPlaceholderText(tr("Production API"));
    form->addRow(tr("Name"), m_name);
    m_group = new QLineEdit(frame);
    m_group->setPlaceholderText(tr("Production"));
    form->addRow(tr("Folder"), m_group);
    m_host = new QLineEdit(frame);
    m_host->setPlaceholderText(tr("api.example.com"));
    form->addRow(tr("Host"), m_host);
    m_port = new QSpinBox(frame);
    m_port->setRange(1, 65535);
    m_port->setValue(22);
    form->addRow(tr("Port"), m_port);
    m_username = new QLineEdit(frame);
    m_username->setPlaceholderText(tr("deploy"));
    form->addRow(tr("Username"), m_username);
    m_authentication = new QComboBox(frame);
    m_authentication->addItem(tr("SSH agent"), QStringLiteral("agent"));
    m_authentication->addItem(tr("Private key"), QStringLiteral("key"));
    m_authentication->addItem(tr("Password prompt"), QStringLiteral("password"));
    form->addRow(tr("Authentication"), m_authentication);

    auto *identityRow = new QHBoxLayout;
    identityRow->setContentsMargins(0, 0, 0, 0);
    m_identityFile = new QLineEdit(frame);
    m_identityFile->setPlaceholderText(tr("~/.ssh/id_ed25519"));
    auto *browse = new QToolButton(frame);
    browse->setText(QStringLiteral("…"));
    browse->setToolTip(tr("Choose private key"));
    connect(browse, &QToolButton::clicked, this, &ProfileDialog::chooseIdentityFile);
    auto *generate = new QPushButton(tr("Generate"), frame);
    generate->setToolTip(tr("Generate a new Ed25519 key with owner-only permissions"));
    connect(generate, &QPushButton::clicked, this, &ProfileDialog::generateIdentityFile);
    identityRow->addWidget(m_identityFile);
    identityRow->addWidget(browse);
    identityRow->addWidget(generate);
    form->addRow(tr("Private key"), identityRow);

    m_secret = new QLineEdit(frame);
    m_secret->setEchoMode(QLineEdit::Password);
    m_secret->setPlaceholderText(tr("Optional; leave blank to keep the saved secret"));
    form->addRow(tr("Password / key passphrase"), m_secret);

    m_jumpHosts = new QLineEdit(frame);
    m_jumpHosts->setPlaceholderText(tr("edge, bastion.example.com"));
    form->addRow(tr("Jump hosts"), m_jumpHosts);
    m_tags = new QLineEdit(frame);
    m_tags->setPlaceholderText(tr("production, database"));
    form->addRow(tr("Tags"), m_tags);
    m_localForwards = new QLineEdit(frame);
    m_localForwards->setPlaceholderText(tr("127.0.0.1:5432:db.internal:5432"));
    form->addRow(tr("Local forwards"), m_localForwards);
    m_remoteForwards = new QLineEdit(frame);
    m_remoteForwards->setPlaceholderText(tr("8080:127.0.0.1:8080"));
    form->addRow(tr("Remote forwards"), m_remoteForwards);
    m_dynamicForwards = new QLineEdit(frame);
    m_dynamicForwards->setPlaceholderText(tr("127.0.0.1:1080"));
    form->addRow(tr("SOCKS forwards"), m_dynamicForwards);
    m_remotePath = new QLineEdit(frame);
    m_remotePath->setText(QStringLiteral("/"));
    form->addRow(tr("SFTP start path"), m_remotePath);
    m_hostKeyPolicy = new QComboBox(frame);
    m_hostKeyPolicy->addItem(tr("Ask before trusting a new host"), QStringLiteral("ask"));
    m_hostKeyPolicy->addItem(tr("Accept new hosts, reject changed keys"), QStringLiteral("accept-new"));
    m_hostKeyPolicy->addItem(tr("Require an existing trusted key"), QStringLiteral("yes"));
    form->addRow(tr("Host key policy"), m_hostKeyPolicy);
    m_knownHostsFile = new QLineEdit(frame);
    m_knownHostsFile->setPlaceholderText(tr("Default: ~/.ssh/known_hosts"));
    form->addRow(tr("Known hosts file"), m_knownHostsFile);
    m_agentForwarding = new QCheckBox(tr("Forward SSH agent"), frame);
    form->addRow(QString(), m_agentForwarding);
    m_compression = new QCheckBox(tr("Enable compression"), frame);
    form->addRow(QString(), m_compression);
    m_legacyCompatibility = new QCheckBox(tr("Legacy server compatibility (weak cryptography)"), frame);
    m_legacyCompatibility->setToolTip(tr("Enables retired algorithms only for this profile using Waypane's isolated SSH runtime."));
    form->addRow(QString(), m_legacyCompatibility);
    auto *scroll = new QScrollArea(frame);
    scroll->setObjectName(QStringLiteral("profileScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(formWidget);
    layout->addWidget(scroll, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, frame);
    connect(buttons, &QDialogButtonBox::accepted, this, &ProfileDialog::validateAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_authentication, &QComboBox::currentIndexChanged, this, [this] {
        m_identityFile->setEnabled(m_authentication->currentData().toString() == QStringLiteral("key"));
        m_secret->setEnabled(m_authentication->currentData().toString() != QStringLiteral("agent"));
    });
    m_secret->setEnabled(false);
}

void ProfileDialog::setProfile(const Waypane::ConnectionProfile &profile)
{
    m_id = profile.id;
    m_secretId = profile.secretId;
    m_sshConfigAlias = profile.sshConfigAlias;
    m_name->setText(profile.name);
    m_group->setText(profile.group);
    m_host->setText(profile.host);
    m_port->setValue(profile.port);
    m_username->setText(profile.username);
    const int authIndex = m_authentication->findData(profile.authentication);
    m_authentication->setCurrentIndex(authIndex < 0 ? 0 : authIndex);
    m_identityFile->setText(profile.identityFile);
    m_jumpHosts->setText(profile.jumpHosts.join(QStringLiteral(", ")));
    m_tags->setText(profile.tags.join(QStringLiteral(", ")));
    m_localForwards->setText(profile.localForwards.join(QStringLiteral(", ")));
    m_remoteForwards->setText(profile.remoteForwards.join(QStringLiteral(", ")));
    m_dynamicForwards->setText(profile.dynamicForwards.join(QStringLiteral(", ")));
    m_remotePath->setText(profile.remotePath);
    const int policyIndex = m_hostKeyPolicy->findData(profile.hostKeyPolicy);
    m_hostKeyPolicy->setCurrentIndex(policyIndex < 0 ? 0 : policyIndex);
    m_knownHostsFile->setText(profile.knownHostsFile);
    m_agentForwarding->setChecked(profile.agentForwarding);
    m_compression->setChecked(profile.compression);
    m_legacyCompatibility->setChecked(profile.legacyCompatibility);
}

Waypane::ConnectionProfile ProfileDialog::profile() const
{
    Waypane::ConnectionProfile result;
    result.id = m_id;
    result.secretId = m_secretId;
    result.sshConfigAlias = m_sshConfigAlias;
    result.name = m_name->text().trimmed();
    result.group = m_group->text().trimmed();
    result.host = m_host->text().trimmed();
    result.port = static_cast<quint16>(m_port->value());
    result.username = m_username->text().trimmed();
    result.authentication = m_authentication->currentData().toString();
    result.identityFile = m_identityFile->text().trimmed();
    result.jumpHosts = m_jumpHosts->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    result.tags = m_tags->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    result.localForwards = m_localForwards->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    result.remoteForwards = m_remoteForwards->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    result.dynamicForwards = m_dynamicForwards->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString &item : result.jumpHosts) {
        item = item.trimmed();
    }
    for (QString &item : result.tags) {
        item = item.trimmed();
    }
    for (QStringList *items : {&result.localForwards, &result.remoteForwards, &result.dynamicForwards}) {
        for (QString &item : *items) {
            item = item.trimmed();
        }
    }
    result.remotePath = m_remotePath->text().trimmed();
    if (result.remotePath.isEmpty()) {
        result.remotePath = QStringLiteral("/");
    }
    result.hostKeyPolicy = m_hostKeyPolicy->currentData().toString();
    result.knownHostsFile = m_knownHostsFile->text().trimmed();
    result.agentForwarding = m_agentForwarding->isChecked();
    result.compression = m_compression->isChecked();
    result.legacyCompatibility = m_legacyCompatibility->isChecked();
    return result;
}

QString ProfileDialog::secret() const
{
    return m_secret->text();
}

void ProfileDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->position().y() <= 48 && windowHandle()) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void ProfileDialog::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();
}

void ProfileDialog::chooseIdentityFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose private key"), QDir::homePath() + QStringLiteral("/.ssh"));
    if (!path.isEmpty()) {
        m_identityFile->setText(path);
        const int keyIndex = m_authentication->findData(QStringLiteral("key"));
        m_authentication->setCurrentIndex(keyIndex);
    }
}

void ProfileDialog::generateIdentityFile()
{
    const QString sshKeygen = Waypane::SshRuntime::tool(QStringLiteral("ssh-keygen"));
    if (sshKeygen.isEmpty()) {
        QMessageBox::warning(this, tr("Generate SSH key"), tr("ssh-keygen is not installed."));
        return;
    }
    const QString sshDirectory = QDir::homePath() + QStringLiteral("/.ssh");
    if (!QDir().mkpath(sshDirectory)) {
        QMessageBox::warning(this, tr("Generate SSH key"), tr("Could not create %1.").arg(sshDirectory));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Create Ed25519 private key"), sshDirectory + QStringLiteral("/id_ed25519_waypane"));
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo::exists(path) || QFileInfo::exists(path + QStringLiteral(".pub"))) {
        QMessageBox::warning(this, tr("Generate SSH key"), tr("Choose a new filename; Waypane will not overwrite an existing key."));
        return;
    }
    QProcess process;
    process.setProgram(sshKeygen);
    process.setArguments({QStringLiteral("-q"), QStringLiteral("-t"), QStringLiteral("ed25519"), QStringLiteral("-a"), QStringLiteral("64"),
                          QStringLiteral("-C"), QStringLiteral("waypane@") + QSysInfo::machineHostName(), QStringLiteral("-N"), QString(),
                          QStringLiteral("-f"), path});
    process.start();
    if (!process.waitForFinished(30000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QMessageBox::warning(this, tr("Generate SSH key"), tr("ssh-keygen failed: %1").arg(QString::fromUtf8(process.readAllStandardError()).trimmed()));
        return;
    }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    m_identityFile->setText(path);
    m_authentication->setCurrentIndex(m_authentication->findData(QStringLiteral("key")));
    QMessageBox::information(this,
                             tr("SSH key created"),
                             tr("Private key: %1\nPublic key: %2.pub\n\nThe private key is unencrypted and readable only by your user. Copy the public key to the server to authorize it.").arg(path, path));
}

void ProfileDialog::validateAndAccept()
{
    const Waypane::ConnectionProfile value = profile();
    QString error;
    if (!value.isValid(&error)) {
        QMessageBox::warning(this, tr("Invalid connection"), error);
        return;
    }
    if (value.authentication == QStringLiteral("key") && value.identityFile.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid connection"), tr("Choose a private key or select another authentication method."));
        return;
    }
    accept();
}
