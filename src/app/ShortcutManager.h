// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>

class QAction;

namespace Donut
{
/// Registers the manual-activation shortcut with KDE's kglobalaccel and
/// refuses sequences that another component already owns.
class ShortcutManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString activeShortcut READ activeShortcut NOTIFY statusChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit ShortcutManager(QObject *parent = nullptr);

    /// Empty if the sequence is free (or already ours), otherwise who owns it.
    Q_INVOKABLE QString conflictFor(const QString &sequence) const;
    /// Applies the sequence; returns false and keeps the old one on conflict.
    bool apply(const QString &sequence);

    QString activeShortcut() const;
    QString status() const;

Q_SIGNALS:
    void activated();
    void statusChanged();

private:
    QAction *m_action;
    QString m_active;
    QString m_status;
};
}
