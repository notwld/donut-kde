// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "DragBackend.h"

class QDBusServiceWatcher;

namespace Donut
{
/*
 * Listens to the "donutdragwatch" KWin effect over the session bus.
 *
 * What it really sees: the start, motion-based activation and end of every
 * pointer drag-and-drop session handled by KWin (any Wayland or Xwayland
 * client), as positions and a dropped flag. It does NOT see the MIME types or
 * URLs: KWin 6.8 does not install the AbstractDataSource header, so the effect
 * cannot read them. The payload is obtained later through the normal Wayland
 * data-device protocol once the drag enters Donut's own overlay surface.
 */
class KWinBackend : public DragBackend
{
    Q_OBJECT

public:
    explicit KWinBackend(QObject *parent = nullptr);

    bool isAvailable() const override;
    QString backendName() const override;
    QString modeDescription() const override;
    Capabilities capabilities() const override;
    void start() override;
    void requestCursorPosition(std::function<void(std::optional<QPointF>)> callback) override;

    QString effectKWinVersion() const;
    int effectProtocolVersion() const;
    QString unavailableReason() const;

    /// Asks KWin to re-read donutrc in the effect (after settings were saved).
    static void reconfigureEffect();

private Q_SLOTS:
    void onDragStarted(double x, double y);
    void onDragActivated(double x, double y);
    void onDragEnded(bool dropped);
    void probe();

private:
    void setAvailable(bool available, const QString &reason = {});

    QDBusServiceWatcher *m_watcher = nullptr;
    bool m_available = false;
    QString m_reason = tr("checking for the KWin effect…");
    QString m_effectKWinVersion;
    int m_protocolVersion = 0;
};
}
