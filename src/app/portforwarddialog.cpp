// SPDX-License-Identifier: GPL-3.0-or-later
#include "portforwarddialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

PortForwardDialog::PortForwardDialog(const Waypane::ConnectionProfile &profile, QWidget *parent)
    : QDialog(parent)
    , m_profile(profile)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    setMinimumSize(620, 600);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);
    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("dialogFrame"));
    outer->addWidget(frame);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(22, 0, 22, 22);
    layout->setSpacing(10);

    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel(tr("Port forwarding · %1").arg(profile.name), frame);
    title->setObjectName(QStringLiteral("dialogTitle"));
    auto *close = new QToolButton(frame);
    close->setObjectName(QStringLiteral("windowCloseButton"));
    close->setText(QStringLiteral("×"));
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(close);
    layout->addLayout(titleRow);

    auto addEditor = [layout, frame](const QString &title, const QString &help, const QStringList &values) {
        auto *label = new QLabel(title, frame);
        label->setObjectName(QStringLiteral("sectionLabel"));
        layout->addWidget(label);
        auto *editor = new QPlainTextEdit(frame);
        editor->setPlainText(values.join(QLatin1Char('\n')));
        editor->setPlaceholderText(help);
        editor->setMaximumBlockCount(100);
        layout->addWidget(editor, 1);
        return editor;
    };
    m_local = addEditor(tr("LOCAL FORWARDS"), tr("One per line, for example 127.0.0.1:5432:db.internal:5432"), profile.localForwards);
    m_remote = addEditor(tr("REMOTE FORWARDS"), tr("One per line, for example 8080:127.0.0.1:8080"), profile.remoteForwards);
    m_dynamic = addEditor(tr("SOCKS FORWARDS"), tr("One per line, for example 127.0.0.1:1080"), profile.dynamicForwards);

    auto *help = new QLabel(tr("Save updates the connection. Save & start opens a visible tunnel session; closing its terminal tab stops all forwards in that session."), frame);
    help->setObjectName(QStringLiteral("mutedLabel"));
    help->setWordWrap(true);
    layout->addWidget(help);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, frame);
    auto *save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    auto *start = buttons->addButton(tr("Save && start"), QDialogButtonBox::ActionRole);
    start->setObjectName(QStringLiteral("primaryButton"));
    connect(save, &QPushButton::clicked, this, &PortForwardDialog::saveOnly);
    connect(start, &QPushButton::clicked, this, &PortForwardDialog::saveAndStart);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QStringList PortForwardDialog::lines(const QPlainTextEdit *editor)
{
    QStringList result;
    for (QString line : editor->toPlainText().split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (!line.isEmpty()) {
            result.append(line);
        }
    }
    return result;
}

Waypane::ConnectionProfile PortForwardDialog::profile() const
{
    Waypane::ConnectionProfile result = m_profile;
    result.localForwards = lines(m_local);
    result.remoteForwards = lines(m_remote);
    result.dynamicForwards = lines(m_dynamic);
    return result;
}

bool PortForwardDialog::startRequested() const
{
    return m_startRequested;
}

void PortForwardDialog::saveOnly()
{
    finish(false);
}

void PortForwardDialog::saveAndStart()
{
    finish(true);
}

void PortForwardDialog::finish(bool start)
{
    m_startRequested = start;
    accept();
}
