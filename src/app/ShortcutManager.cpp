// SPDX-License-Identifier: GPL-2.0-or-later
#include "ShortcutManager.h"
#include "Logging.h"

#include <KGlobalAccel>
#include <KGlobalShortcutInfo>

#include <QAction>
#include <QCoreApplication>
#include <QKeySequence>

using namespace Qt::StringLiterals;

namespace Donut
{
ShortcutManager::ShortcutManager(QObject *parent)
    : QObject(parent)
    , m_action(new QAction(tr("Open Donut at the pointer"), this))
{
    m_action->setObjectName(u"activate-donut"_s);
    connect(m_action, &QAction::triggered, this, &ShortcutManager::activated);
}

QString ShortcutManager::conflictFor(const QString &sequence) const
{
    const QKeySequence seq = QKeySequence::fromString(sequence, QKeySequence::PortableText);
    if (seq.isEmpty()) {
        return {};
    }
    const QString component = QCoreApplication::applicationName();
    if (KGlobalAccel::isGlobalShortcutAvailable(seq, component)) {
        return {};
    }
    const QList<KGlobalShortcutInfo> owners = KGlobalAccel::globalShortcutsByKey(seq);
    for (const KGlobalShortcutInfo &info : owners) {
        if (info.componentUniqueName() == component) {
            continue; // it is ours
        }
        return tr("%1 is already used by \"%2\" (%3)").arg(sequence, info.friendlyName(), info.componentFriendlyName());
    }
    return {};
}

bool ShortcutManager::apply(const QString &sequence)
{
    const QKeySequence seq = QKeySequence::fromString(sequence, QKeySequence::PortableText);
    if (const QString conflict = conflictFor(sequence); !conflict.isEmpty()) {
        m_status = conflict;
        qCWarning(lcCore) << "shortcut conflict:" << conflict;
        Q_EMIT statusChanged();
        return false;
    }
    const QList<QKeySequence> list = seq.isEmpty() ? QList<QKeySequence>{} : QList<QKeySequence>{seq};
    KGlobalAccel::self()->setDefaultShortcut(m_action, list, KGlobalAccel::NoAutoloading);
    KGlobalAccel::self()->setShortcut(m_action, list, KGlobalAccel::NoAutoloading);
    m_active = seq.toString(QKeySequence::PortableText);
    m_status = m_active.isEmpty() ? tr("No shortcut") : tr("%1 opens Donut").arg(m_active);
    Q_EMIT statusChanged();
    return true;
}

QString ShortcutManager::activeShortcut() const
{
    return m_active;
}

QString ShortcutManager::status() const
{
    return m_status;
}
}
