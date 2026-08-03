// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/connectionprofile.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

class ProfileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProfileDialog(QWidget *parent = nullptr);

    void setProfile(const Waypane::ConnectionProfile &profile);
    [[nodiscard]] Waypane::ConnectionProfile profile() const;
    [[nodiscard]] QString secret() const;

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void chooseIdentityFile();
    void generateIdentityFile();
    void validateAndAccept();

private:
    QString m_id;
    QString m_secretId;
    QString m_sshConfigAlias;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_group = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QLineEdit *m_username = nullptr;
    QComboBox *m_authentication = nullptr;
    QLineEdit *m_identityFile = nullptr;
    QLineEdit *m_secret = nullptr;
    QLineEdit *m_jumpHosts = nullptr;
    QLineEdit *m_tags = nullptr;
    QLineEdit *m_localForwards = nullptr;
    QLineEdit *m_remoteForwards = nullptr;
    QLineEdit *m_dynamicForwards = nullptr;
    QLineEdit *m_remotePath = nullptr;
    QComboBox *m_hostKeyPolicy = nullptr;
    QLineEdit *m_knownHostsFile = nullptr;
    QCheckBox *m_agentForwarding = nullptr;
    QCheckBox *m_compression = nullptr;
    QCheckBox *m_legacyCompatibility = nullptr;
};
