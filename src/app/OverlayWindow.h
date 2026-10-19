// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QPointF>
#include <QPointer>

class QQmlEngine;
class QQuickWindow;
class QScreen;

namespace LayerShellQt
{
class Window;
}

namespace Donut
{
class DonutController;
class SettingsManager;

/*
 * The always-on-top donut surface used by the daemon.
 *
 * On Wayland it is a wlr-layer-shell surface on the overlay layer, positioned
 * with anchors + margins on the output under the activation point. It is a
 * regular Wayland client surface, so while a drag hovers it, KWin delivers the
 * standard wl_data_device enter/motion/drop events and the dragged URLs.
 * Its input region is limited to the donut circle so the rest of the screen
 * keeps receiving drags and clicks normally.
 */
class OverlayWindow : public QObject
{
    Q_OBJECT

public:
    OverlayWindow(QQmlEngine *engine, DonutController *controller, SettingsManager *settings, QObject *parent = nullptr);
    ~OverlayWindow() override;

    bool isValid() const;
    QQuickWindow *window() const;
    bool usesLayerShell() const;

    /// Margin around the ring for glow, plus room for the status toast below.
    static constexpr int GlowMargin = 28;
    static constexpr int ToastSpace = 64;

private:
    void place(const QPointF &globalPosition, bool interactive);
    void hide();
    QScreen *screenFor(const QPointF &globalPosition) const;

    QPointer<QQuickWindow> m_window;
    LayerShellQt::Window *m_layer = nullptr;
    DonutController *m_controller;
    SettingsManager *m_settings;
};
}
