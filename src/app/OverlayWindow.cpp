// SPDX-License-Identifier: GPL-2.0-or-later
#include "OverlayWindow.h"
#include "DonutController.h"
#include "Logging.h"
#include "SettingsManager.h"

#include <KWindowEffects>
#include <LayerShellQt/Window>

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QRegion>
#include <QScreen>

using namespace Qt::StringLiterals;

namespace Donut
{
OverlayWindow::OverlayWindow(QQmlEngine *engine, DonutController *controller, SettingsManager *settings, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_settings(settings)
{
    QQmlComponent component(engine);
    component.loadFromModule("org.kde.donut", "Main");
    QObject *object = component.createWithInitialProperties({
        {u"controller"_s, QVariant::fromValue(controller)},
        {u"settings"_s, QVariant::fromValue(settings)},
    });
    m_window = qobject_cast<QQuickWindow *>(object);
    if (!m_window) {
        qCCritical(lcUi) << "failed to create overlay:" << component.errorString();
        delete object;
        return;
    }

    if (usesLayerShell()) {
        // Must be configured before the platform window is created (first show).
        m_layer = LayerShellQt::Window::get(m_window);
        m_layer->setLayer(LayerShellQt::Window::LayerOverlay);
        m_layer->setScope(u"donut"_s);
        m_layer->setExclusiveZone(-1);
        m_layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorLeft));
        m_layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        m_layer->setActivateOnShow(false);
        m_layer->setCloseOnDismissed(false);
        m_layer->setWantsToBeOnActiveScreen(false);
    } else {
        // X11 and other platforms: a frameless always-on-top tool window.
        m_window->setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    }

    connect(controller, &DonutController::openRequested, this, &OverlayWindow::place);
    connect(controller, &DonutController::hidden, this, &OverlayWindow::hide);
}

OverlayWindow::~OverlayWindow()
{
    delete m_window;
}

bool OverlayWindow::isValid() const
{
    return m_window;
}

QQuickWindow *OverlayWindow::window() const
{
    return m_window;
}

bool OverlayWindow::usesLayerShell() const
{
    return QGuiApplication::platformName() == u"wayland"_s;
}

QScreen *OverlayWindow::screenFor(const QPointF &globalPosition) const
{
    if (QScreen *s = QGuiApplication::screenAt(globalPosition.toPoint())) {
        return s;
    }
    return QGuiApplication::primaryScreen();
}

void OverlayWindow::place(const QPointF &globalPosition, bool interactive)
{
    if (!m_window) {
        return;
    }
    QScreen *screen = screenFor(globalPosition);
    if (!screen) {
        qCWarning(lcUi) << "no screen to show the donut on";
        return;
    }

    const int radius = m_settings->outerRadius();
    const int side = 2 * (radius + GlowMargin);
    const QSize size(side, side + ToastSpace);

    QPointF centre = globalPosition;
    if (m_settings->placement() == u"offset"_s) {
        // Shift the donut up-left so the cursor image (which extends down-right
        // from the hotspot) covers less of the ring.
        const qreal d = m_settings->placementOffset() / std::sqrt(2.0);
        centre -= QPointF(d, d);
    }

    // Clamp the whole window onto the screen; near edges the pointer is then
    // no longer exactly at the hub, which is preferable to a clipped ring.
    const QRect geo = screen->geometry();
    QPoint topLeft = (centre - QPointF(side / 2.0, side / 2.0)).toPoint();
    topLeft.setX(std::clamp(topLeft.x(), geo.left(), std::max(geo.left(), geo.right() + 1 - size.width())));
    topLeft.setY(std::clamp(topLeft.y(), geo.top(), std::max(geo.top(), geo.bottom() + 1 - size.height())));

    const bool screenChanged = m_window->screen() != screen;
    if (m_window->isVisible() && screenChanged) {
        m_window->hide(); // layer surfaces cannot move between outputs while mapped
    }

    m_window->setScreen(screen);
    m_window->resize(size);
    m_window->setMask(QRegion(QRect(GlowMargin - 2, GlowMargin - 2, 2 * radius + 4, 2 * radius + 4), QRegion::Ellipse));

    if (m_layer) {
        m_layer->setScreen(screen);
        m_layer->setDesiredSize(size);
        m_layer->setMargins(QMargins(topLeft.x() - geo.left(), topLeft.y() - geo.top(), 0, 0));
        m_layer->setKeyboardInteractivity(interactive ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                                                      : LayerShellQt::Window::KeyboardInteractivityNone);
        m_layer->setActivateOnShow(interactive);
    } else {
        m_window->setPosition(topLeft);
    }

    qCDebug(lcUi) << "showing donut at" << topLeft << "on" << screen->name() << (interactive ? "(interactive)" : "(drag)");
    m_window->show();
    if (interactive) {
        m_window->requestActivate();
    }
    KWindowEffects::enableBlurBehind(m_window,
                                     m_settings->blurBehind(),
                                     QRegion(QRect(GlowMargin, GlowMargin, 2 * radius, 2 * radius), QRegion::Ellipse));
}

void OverlayWindow::hide()
{
    if (m_window) {
        m_window->hide();
    }
}
}
