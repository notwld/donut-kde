// SPDX-License-Identifier: GPL-2.0-or-later
#include "KWinBackend.h"
#include "DonutConfig.h"
#include "Logging.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>

using namespace Qt::StringLiterals;

namespace Donut
{
namespace
{
const QString KWinService = u"org.kde.KWin"_s;

QDBusMessage effectCall(const QString &method)
{
    return QDBusMessage::createMethodCall(KWinService,
                                          QString::fromLatin1(Config::EffectDBusPath),
                                          QString::fromLatin1(Config::EffectDBusInterface),
                                          method);
}
}

KWinBackend::KWinBackend(QObject *parent)
    : DragBackend(parent)
{
}

bool KWinBackend::isAvailable() const
{
    return m_available;
}

QString KWinBackend::backendName() const
{
    return u"kwin"_s;
}

QString KWinBackend::modeDescription() const
{
    if (!m_available) {
        return tr("Native compositor integration unavailable: %1").arg(m_reason);
    }
    return tr("Native compositor integration (KWin effect): sees every pointer drag; file names become visible when the drag enters the donut");
}

DragBackend::Capabilities KWinBackend::capabilities() const
{
    return Capability::GlobalDragLifecycle | Capability::CursorPosition | Capability::ManualActivation;
}

QString KWinBackend::effectKWinVersion() const
{
    return m_effectKWinVersion;
}

int KWinBackend::effectProtocolVersion() const
{
    return m_protocolVersion;
}

QString KWinBackend::unavailableReason() const
{
    return m_reason;
}

void KWinBackend::start()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString path = QString::fromLatin1(Config::EffectDBusPath);
    const QString iface = QString::fromLatin1(Config::EffectDBusInterface);
    bus.connect(KWinService, path, iface, u"dragStarted"_s, this, SLOT(onDragStarted(double, double)));
    bus.connect(KWinService, path, iface, u"dragActivated"_s, this, SLOT(onDragActivated(double, double)));
    bus.connect(KWinService, path, iface, u"dragEnded"_s, this, SLOT(onDragEnded(bool)));
    // The effect announces itself when (re)loaded, so no polling is needed.
    bus.connect(KWinService, path, iface, u"ready"_s, this, SLOT(probe()));

    m_watcher = new QDBusServiceWatcher(KWinService, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &KWinBackend::probe);
    probe();
}

void KWinBackend::probe()
{
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(effectCall(u"protocolVersion"_s), 2000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<int> reply = *w;
        if (reply.isError()) {
            setAvailable(false, tr("the \"%1\" KWin effect is not loaded").arg(QString::fromLatin1(Config::EffectId)));
            qCInfo(lcBackend) << "KWin effect not reachable:" << reply.error().name();
            return;
        }
        m_protocolVersion = reply.value();
        if (m_protocolVersion != Config::EffectProtocolVersion) {
            setAvailable(false, tr("KWin effect protocol %1, expected %2 (rebuild/reinstall the effect)").arg(m_protocolVersion).arg(Config::EffectProtocolVersion));
            return;
        }
        auto *versionWatcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(effectCall(u"kwinVersion"_s), 2000), this);
        connect(versionWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *vw) {
            vw->deleteLater();
            const QDBusPendingReply<QString> v = *vw;
            m_effectKWinVersion = v.isError() ? QString() : v.value();
            qCInfo(lcBackend) << "KWin effect available, built for KWin" << m_effectKWinVersion;
            setAvailable(true);
        });
    });
}

void KWinBackend::setAvailable(bool available, const QString &reason)
{
    if (m_available == available && m_reason == reason) {
        return;
    }
    m_available = available;
    m_reason = reason;
    Q_EMIT availabilityChanged();
}

void KWinBackend::requestCursorPosition(std::function<void(std::optional<QPointF>)> callback)
{
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(effectCall(u"cursorPosition"_s), 1000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [callback = std::move(callback)](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QPointF> reply = *w;
        callback(reply.isError() ? std::nullopt : std::optional<QPointF>(reply.value()));
    });
}

void KWinBackend::reconfigureEffect()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(KWinService, u"/Effects"_s, u"org.kde.kwin.Effects"_s, u"reconfigureEffect"_s);
    msg << QString::fromLatin1(Config::EffectId);
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

void KWinBackend::onDragStarted(double x, double y)
{
    qCDebug(lcBackend) << "drag started at" << x << y;
    DragPayload payload;
    payload.position = {x, y};
    payload.isExternalDrag = true;
    payload.payloadKnown = false;
    Q_EMIT dragStarted(payload);
}

void KWinBackend::onDragActivated(double x, double y)
{
    qCDebug(lcBackend) << "activation requested at" << x << y;
    Q_EMIT activationRequested({x, y});
}

void KWinBackend::onDragEnded(bool dropped)
{
    qCDebug(lcBackend) << "drag ended, dropped =" << dropped;
    Q_EMIT dragFinished(dropped);
}
}
