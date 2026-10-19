// SPDX-License-Identifier: GPL-2.0-or-later
#include "donutdragwatcheffect.h"
#include "DonutConfig.h"

#include <config-kwin.h>
#include <effect/effecthandler.h>
#include <wayland/seat.h>
#include <wayland_server.h>

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QLoggingCategory>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcDonutKWin, "donut.kwin", QtInfoMsg)

namespace Donut
{
DragWatchEffect::DragWatchEffect()
{
    m_clock.start();
    m_dwellTimer.setSingleShot(true);
    m_dwellTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_dwellTimer, &QTimer::timeout, this, &DragWatchEffect::handleDwellTimeout);

    reconfigure(ReconfigureAll);

    if (KWin::WaylandServer *server = KWin::waylandServer()) {
        m_seat = server->seat();
    }
    if (m_seat) {
        // SeatInterface::dragMoved is not emitted for pointer drags in KWin 6.8
        // (verified in a nested session), so motion comes from the public
        // EffectsHandler::mouseChanged signal, connected only while a drag runs.
        connect(m_seat, &KWin::SeatInterface::dragStarted, this, &DragWatchEffect::handleDragStarted);
        connect(m_seat, &KWin::SeatInterface::dragDropped, this, &DragWatchEffect::handleDragDropped);
        connect(m_seat, &KWin::SeatInterface::dragEnded, this, &DragWatchEffect::handleDragEnded);
    } else {
        qCWarning(lcDonutKWin) << "no Wayland seat; Donut drag watching is inactive";
    }

    m_dbusRegistered = QDBusConnection::sessionBus().registerObject(QString::fromLatin1(Config::EffectDBusPath),
                                                                     this,
                                                                     QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);
    if (!m_dbusRegistered) {
        qCWarning(lcDonutKWin) << "could not register" << Config::EffectDBusPath << "on the session bus";
    }
    qCInfo(lcDonutKWin) << "loaded, protocol" << Config::EffectProtocolVersion << "built for KWin" << KWIN_PLUGIN_VERSION_STRING;
    // Let a running Donut app know it can stop reporting "effect not loaded".
    QTimer::singleShot(0, this, &DragWatchEffect::ready);
}

DragWatchEffect::~DragWatchEffect()
{
    if (m_dbusRegistered) {
        QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(Config::EffectDBusPath));
    }
}

bool DragWatchEffect::supported()
{
    return KWin::waylandServer() != nullptr;
}

void DragWatchEffect::reconfigure(ReconfigureFlags)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString::fromLatin1(Config::FileName), KConfig::SimpleConfig);
    config->reparseConfiguration();
    const KConfigGroup general = config->group(QString::fromLatin1(Config::GroupGeneral));
    const KConfigGroup trigger = config->group(QString::fromLatin1(Config::GroupTrigger));

    m_enabled = general.readEntry(Config::KeyEnabled, Config::DefaultEnabled);

    ActivationTracker::Config c;
    const QString mode = trigger.readEntry(Config::KeyTriggerMode, QString::fromLatin1(Config::DefaultTriggerMode));
    c.mode = mode == u"distance"_s ? ActivationTracker::Mode::Distance
        : mode == u"shortcut"_s    ? ActivationTracker::Mode::Manual
                                   : ActivationTracker::Mode::Dwell;
    c.threshold = std::clamp(trigger.readEntry(Config::KeyThreshold, Config::DefaultThreshold), Config::MinThreshold, Config::MaxThreshold);
    c.dwellMs = std::clamp(trigger.readEntry(Config::KeyDwellMs, Config::DefaultDwellMs), Config::MinDwellMs, Config::MaxDwellMs);
    m_tracker.setConfig(c);
    qCInfo(lcDonutKWin) << "config: enabled" << m_enabled << "mode" << mode << "threshold" << c.threshold << "dwell" << c.dwellMs;
}

bool DragWatchEffect::isActive() const
{
    return false; // never takes part in painting
}

int DragWatchEffect::protocolVersion() const
{
    return Config::EffectProtocolVersion;
}

QString DragWatchEffect::kwinVersion() const
{
    return QStringLiteral(KWIN_PLUGIN_VERSION_STRING);
}

bool DragWatchEffect::dragActive() const
{
    return m_seat && m_seat->isDrag();
}

QPointF DragWatchEffect::cursorPosition() const
{
    return KWin::effects->cursorPos();
}

QString DragWatchEffect::triggerMode() const
{
    switch (m_tracker.config().mode) {
    case ActivationTracker::Mode::Distance:
        return u"distance"_s;
    case ActivationTracker::Mode::Manual:
        return u"shortcut"_s;
    case ActivationTracker::Mode::Dwell:
        break;
    }
    return u"dwell"_s;
}

void DragWatchEffect::handleDragStarted()
{
    m_tracking = false;
    m_dropped = false;
    if (!m_enabled || !m_seat) {
        return;
    }
    if (!m_seat->isDragPointer()) {
        qCDebug(lcDonutKWin) << "ignoring non-pointer drag";
        return;
    }
    if (m_seat->xdgTopleveldrag()) {
        qCDebug(lcDonutKWin) << "ignoring window (toplevel) drag";
        return;
    }
    m_tracking = true;
    const QPointF pos = KWin::effects->cursorPos();
    m_tracker.begin(pos, m_clock.elapsed());
    m_motionConnection = connect(KWin::effects, &KWin::EffectsHandler::mouseChanged, this, [this](const QPointF &position) {
        handleDragMoved(position);
    });
    qCDebug(lcDonutKWin) << "drag started at" << pos;
    Q_EMIT dragStarted(pos.x(), pos.y());
}

void DragWatchEffect::handleDragMoved(const QPointF &position)
{
    if (m_tracking) {
        apply(m_tracker.motion(position, m_clock.elapsed()));
    }
}

void DragWatchEffect::handleDwellTimeout()
{
    if (m_tracking) {
        apply(m_tracker.dwellTimeout(m_clock.elapsed()));
    }
}

void DragWatchEffect::apply(ActivationTracker::Result result)
{
    switch (result) {
    case ActivationTracker::Result::None:
        break;
    case ActivationTracker::Result::ArmDwellTimer:
        m_dwellTimer.start(int(m_tracker.dwellRemainingMs(m_clock.elapsed())));
        break;
    case ActivationTracker::Result::Activate: {
        m_dwellTimer.stop();
        const QPointF pos = m_tracker.activationPosition();
        qCDebug(lcDonutKWin) << "activating at" << pos;
        Q_EMIT dragActivated(pos.x(), pos.y());
        break;
    }
    }
}

void DragWatchEffect::handleDragDropped()
{
    m_dropped = true;
}

void DragWatchEffect::handleDragEnded()
{
    m_dwellTimer.stop();
    disconnect(m_motionConnection);
    if (m_tracking) {
        qCDebug(lcDonutKWin) << "drag ended, dropped =" << m_dropped;
        Q_EMIT dragEnded(m_dropped);
    }
    m_tracking = false;
    m_dropped = false;
    m_tracker.reset();
}
}
