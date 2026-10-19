// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "ActivationTracker.h"

#include <effect/effect.h>

#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>

namespace KWin
{
class SeatInterface;
}

namespace Donut
{
/*
 * Observes KWin's drag-and-drop sessions and tells the Donut app when a drag
 * should open the radial menu.
 *
 * It never paints, never grabs input, never touches the drag source or target
 * and never reads the dragged data (the installed KWin headers do not allow
 * that anyway). It only forwards positions and a "dropped" flag over D-Bus.
 */
class DragWatchEffect : public KWin::Effect
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Donut.DragWatch")

public:
    DragWatchEffect();
    ~DragWatchEffect() override;

    static bool supported();

    void reconfigure(ReconfigureFlags flags) override;
    bool isActive() const override;

public Q_SLOTS:
    Q_SCRIPTABLE int protocolVersion() const;
    Q_SCRIPTABLE QString kwinVersion() const;
    Q_SCRIPTABLE bool dragActive() const;
    Q_SCRIPTABLE QPointF cursorPosition() const;
    Q_SCRIPTABLE QString triggerMode() const;

Q_SIGNALS:
    Q_SCRIPTABLE void ready();
    Q_SCRIPTABLE void dragStarted(double x, double y);
    Q_SCRIPTABLE void dragActivated(double x, double y);
    Q_SCRIPTABLE void dragEnded(bool dropped);

private:
    void handleDragStarted();
    void handleDragMoved(const QPointF &position);
    void handleDragDropped();
    void handleDragEnded();
    void handleDwellTimeout();
    void apply(ActivationTracker::Result result);

    QPointer<KWin::SeatInterface> m_seat;
    ActivationTracker m_tracker;
    QTimer m_dwellTimer;
    QMetaObject::Connection m_motionConnection;
    QElapsedTimer m_clock;
    bool m_enabled = true;
    bool m_tracking = false;
    bool m_dropped = false;
    bool m_dbusRegistered = false;
};
}
