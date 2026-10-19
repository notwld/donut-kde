// SPDX-License-Identifier: GPL-2.0-or-later
#include "DonutController.h"
#include "DragBackend.h"
#include "FileActionService.h"
#include "FileNaming.h"
#include "Logging.h"
#include "RadialGeometry.h"
#include "SettingsManager.h"

#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace Donut
{
namespace
{
// How long to wait after the compositor reports the drag ended for the Wayland
// drop event (a different socket) to arrive before treating it as "not for us".
constexpr int DragEndGraceMs = 300;
constexpr int SuccessFeedbackMs = 900;
constexpr int CancelFeedbackMs = 600;
constexpr int ErrorFeedbackMs = 2600;
constexpr int RejectedPayloadMs = 450;
constexpr qint64 ReleaseAfterExitMs = 200;
}

DonutController::DonutController(SettingsManager *settings, FileActionService *service, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_service(service)
{
    m_dragEndGrace.setSingleShot(true);
    m_dragEndGrace.setInterval(DragEndGraceMs);
    connect(&m_dragEndGrace, &QTimer::timeout, this, &DonutController::onDragEndGrace);
    m_feedbackTimer.setSingleShot(true);
    connect(&m_feedbackTimer, &QTimer::timeout, this, &DonutController::finishFeedback);
    m_rejectTimer.setSingleShot(true);
    m_rejectTimer.setInterval(RejectedPayloadMs);
    connect(&m_rejectTimer, &QTimer::timeout, this, &DonutController::closeSilently);

    connect(m_settings, &SettingsManager::changed, this, [this] {
        recomputeActions();
        Q_EMIT geometryChanged();
    });
    connect(m_service, &FileActionService::finished, this, &DonutController::onOperationFinished);
    connect(m_service, &FileActionService::progressChanged, this, [this] {
        m_progress = m_service->progress();
        Q_EMIT progressChanged();
    });
    recomputeActions();
}

DonutController::~DonutController() = default;

void DonutController::setBackend(DragBackend *backend)
{
    if (m_backend) {
        disconnect(m_backend, nullptr, this, nullptr);
    }
    m_backend = backend;
    recomputeActions();
    if (backend) {
        connect(backend, &DragBackend::dragStarted, this, &DonutController::onDragStarted);
        connect(backend, &DragBackend::activationRequested, this, &DonutController::onActivationRequested);
        connect(backend, &DragBackend::dragFinished, this, &DonutController::onDragFinished);
        connect(backend, &DragBackend::availabilityChanged, this, &DonutController::backendChanged);
        connect(backend, &DragBackend::backendError, this, [this](const QString &message) {
            qCWarning(lcBackend) << "backend error:" << message;
        });
        qCInfo(lcBackend) << "using backend" << backend->backendName() << "-" << backend->modeDescription();
    }
    Q_EMIT backendChanged();
}

DragBackend *DonutController::backend() const
{
    return m_backend;
}

void DonutController::setPersistent(bool persistent)
{
    m_persistent = persistent;
}

// --- State machine ------------------------------------------------------

bool DonutController::isTransitionAllowed(State from, State to)
{
    switch (from) {
    case State::Idle:
        return to == State::PotentialDrag || to == State::MenuOpening;
    case State::PotentialDrag:
        return to == State::MenuOpening || to == State::Idle;
    case State::MenuOpening:
    case State::MenuActive:
        return to == State::MenuActive || to == State::ActionConfirmed || to == State::Cancelled || to == State::Error || to == State::Idle;
    case State::ActionConfirmed:
        return to == State::OperationRunning || to == State::Cancelled || to == State::Error;
    case State::OperationRunning:
        return to == State::Completed || to == State::Cancelled || to == State::Error;
    case State::Completed:
    case State::Cancelled:
    case State::Error:
        return to == State::Idle;
    }
    return false;
}

bool DonutController::transitionTo(State next)
{
    if (next == m_state) {
        return true;
    }
    if (!isTransitionAllowed(m_state, next)) {
        qCWarning(lcState) << "rejected transition" << stateName(m_state) << "->" << stateName(next);
        return false;
    }
    qCDebug(lcState) << stateName(m_state) << "->" << stateName(next);
    m_state = next;
    Q_EMIT stateChanged();
    return true;
}

bool DonutController::menuAcceptsInput() const
{
    return m_state == State::MenuOpening || m_state == State::MenuActive;
}

// --- Accessors ------------------------------------------------------------

State DonutController::state() const
{
    return m_state;
}

QString DonutController::stateNameString() const
{
    return stateName(m_state);
}

bool DonutController::menuOpen() const
{
    return m_menuOpen;
}

bool DonutController::interactive() const
{
    return m_interactive;
}

bool DonutController::persistent() const
{
    return m_persistent;
}

int DonutController::sectorCount() const
{
    return int(m_ring.size());
}

int DonutController::activeIndex() const
{
    return m_activeIndex;
}

Action DonutController::actionAt(int index) const
{
    return (index >= 0 && index < m_ring.size()) ? m_ring.at(index) : Action::Cancel;
}

bool DonutController::hasPayload() const
{
    return m_payload.hasUrls();
}

bool DonutController::payloadKnown() const
{
    return m_payload.payloadKnown;
}

bool DonutController::payloadRejected() const
{
    return m_payloadRejected;
}

int DonutController::itemCount() const
{
    return int(m_payload.urls.size());
}

QString DonutController::payloadName() const
{
    return m_payload.displayName();
}

QString DonutController::payloadIcon() const
{
    if (m_payload.urls.isEmpty()) {
        return u"document-multiple"_s;
    }
    if (m_payload.urls.size() > 1) {
        return u"document-multiple"_s;
    }
    const QUrl &url = m_payload.urls.constFirst();
    if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir()) {
        return u"folder"_s;
    }
    static const QMimeDatabase db;
    return db.mimeTypeForUrl(url).iconName();
}

const DragPayload &DonutController::payload() const
{
    return m_payload;
}

bool DonutController::dragInside() const
{
    return m_dragInside;
}

double DonutController::progress() const
{
    return m_progress;
}

QString DonutController::statusMessage() const
{
    return m_statusMessage;
}

QString DonutController::statusKind() const
{
    return m_statusKind;
}

QString DonutController::backendName() const
{
    return m_backend ? m_backend->backendName() : u"none"_s;
}

QString DonutController::modeDescription() const
{
    return m_backend ? m_backend->modeDescription() : tr("No backend");
}

bool DonutController::dryRun() const
{
    return m_backend && m_backend->hasCapability(DragBackend::Capability::DryRun);
}

double DonutController::outerRadius() const
{
    return m_settings->outerRadius();
}

double DonutController::innerRadius() const
{
    return m_settings->innerRadius();
}

QPointF DonutController::anchorPosition() const
{
    return m_anchor;
}

QVariantList DonutController::actionsModel() const
{
    QVariantList list;
    for (int i = 0; i < m_ring.size(); ++i) {
        const Action a = m_ring.at(i);
        QVariantMap entry;
        entry[u"action"_s] = int(a);
        entry[u"id"_s] = actionId(a);
        entry[u"label"_s] = actionLabel(a);
        entry[u"icon"_s] = actionIconName(a);
        entry[u"enabled"_s] = isActionEnabled(i);
        entry[u"reason"_s] = i < m_validity.size() ? m_validity.at(i).reason : QString();
        list.append(entry);
    }
    return list;
}

// --- Geometry ---------------------------------------------------------------

int DonutController::sectorAt(qreal dx, qreal dy) const
{
    const RadialHit hit = RadialGeometry::hitTest(dx, dy, innerRadius(), outerRadius(), sectorCount());
    switch (hit.region) {
    case RadialHit::Region::Hub:
        return HubIndex;
    case RadialHit::Region::Outside:
        return OutsideIndex;
    case RadialHit::Region::Ring:
        return hit.sector;
    }
    return OutsideIndex;
}

void DonutController::hoverAt(qreal dx, qreal dy)
{
    if (!menuAcceptsInput()) {
        return;
    }
    const int sector = sectorAt(dx, dy);
    setActiveIndex(sector >= 0 ? sector : HubIndex);
}

void DonutController::clearHover()
{
    setActiveIndex(HubIndex);
}

bool DonutController::isActionEnabled(int index) const
{
    if (index < 0 || index >= m_ring.size()) {
        return false;
    }
    if (m_ring.at(index) == Action::Cancel) {
        return true;
    }
    // Unknown payload (compositor drag not yet over Donut): optimistically enabled.
    if (!m_payload.payloadKnown) {
        return true;
    }
    return index < m_validity.size() && m_validity.at(index).ok;
}

void DonutController::setActiveIndex(int index)
{
    if (m_activeIndex != index) {
        m_activeIndex = index;
        Q_EMIT activeIndexChanged();
    }
}

void DonutController::recomputeActions()
{
    m_ring = m_settings->ringActions();
    m_validity.clear();
    for (Action a : std::as_const(m_ring)) {
        // Dry runs never touch files, so their (illustrative) payload is not validated.
        m_validity.append(m_payload.payloadKnown && !dryRun() ? validateAction(a, m_payload.urls) : Validation{true, {}});
    }
    if (m_activeIndex >= m_ring.size()) {
        m_activeIndex = HubIndex;
        Q_EMIT activeIndexChanged();
    }
    Q_EMIT actionsChanged();
}

// --- Payload ----------------------------------------------------------------

void DonutController::setPayload(const DragPayload &payload)
{
    m_payload = payload;
    recomputeActions();
    Q_EMIT payloadChanged();
}

void DonutController::setPayloadUrls(const QList<QUrl> &urls)
{
    DragPayload p;
    p.urls = urls;
    p.mimeType = u"text/uri-list"_s;
    p.mimeTypes = {p.mimeType};
    p.payloadKnown = true;
    m_payloadRejected = false;
    setPayload(p);
}

bool DonutController::dragEntered(const QStringList &formats, const QString &uriList)
{
    m_dragEndGrace.stop();
    m_rejectTimer.stop();
    setDragInside(true);
    if (!menuAcceptsInput()) {
        return false;
    }
    // Parse text/uri-list ourselves. Qt may prefer the document-portal
    // transfer format, which yields portal paths instead of the originals.
    DragPayload p;
    p.mimeTypes = formats;
    p.urls = parseUriList(uriList.toUtf8());
    p.mimeType = formats.contains(u"text/uri-list"_s) ? u"text/uri-list"_s : formats.value(0);
    p.payloadKnown = true;
    p.isExternalDrag = true;
    p.position = m_anchor;
    m_payloadRejected = p.urls.isEmpty();
    setPayload(p);
    qCDebug(lcUi) << "drag entered:" << formats.size() << "formats," << p.urls.size() << "urls";

    if (m_payloadRejected) {
        // Not a file drag (text, browser URL without file...). Get out of the way.
        setStatus(tr("Only files and folders"), u"info"_s);
        if (!m_persistent) {
            m_rejectTimer.start();
        }
        return false;
    }
    setStatus({}, {});
    return true;
}

void DonutController::dragExited()
{
    setDragInside(false);
    m_sinceExit.start();
    clearHover();
}

void DonutController::setDragInside(bool inside)
{
    if (m_dragInside != inside) {
        m_dragInside = inside;
        Q_EMIT dragInsideChanged();
    }
}

bool DonutController::canDropAt(qreal dx, qreal dy) const
{
    if (!menuAcceptsInput() || m_payloadRejected) {
        return false;
    }
    const int sector = sectorAt(dx, dy);
    if (sector < 0) {
        return false; // hub or outside: refusing makes the source show "no drop"
    }
    if (m_ring.at(sector) == Action::Cancel) {
        return false; // a refused drop is how Cancel reaches the source: nothing is transferred
    }
    return isActionEnabled(sector);
}

bool DonutController::dropAt(qreal dx, qreal dy, const QString &uriList)
{
    m_dragEndGrace.stop();
    setDragInside(false);
    if (!canDropAt(dx, dy)) {
        return false;
    }
    const QList<QUrl> urls = parseUriList(uriList.toUtf8());
    if (!urls.isEmpty() && urls != m_payload.urls) {
        DragPayload p = m_payload;
        p.urls = urls;
        p.payloadKnown = true;
        setPayload(p);
    }
    return confirmAction(sectorAt(dx, dy));
}

// --- Keyboard / click -------------------------------------------------------

void DonutController::selectNext()
{
    if (menuAcceptsInput() && !m_ring.isEmpty()) {
        setActiveIndex(m_activeIndex < 0 ? 0 : (m_activeIndex + 1) % int(m_ring.size()));
    }
}

void DonutController::selectPrevious()
{
    if (menuAcceptsInput() && !m_ring.isEmpty()) {
        const int n = int(m_ring.size());
        setActiveIndex(m_activeIndex < 0 ? n - 1 : (m_activeIndex - 1 + n) % n);
    }
}

bool DonutController::confirmActive()
{
    return confirmAction(m_activeIndex);
}

bool DonutController::confirmAction(int index)
{
    if (!menuAcceptsInput() || index < 0 || index >= m_ring.size()) {
        return false;
    }
    const Action action = m_ring.at(index);
    if (action == Action::Cancel) {
        cancelMenu(tr("Cancelled"));
        return true;
    }
    if (!m_payload.hasUrls()) {
        setStatus(tr("Drop files on the donut first"), u"info"_s);
        return false;
    }
    if (const Validation v = dryRun() ? Validation{true, {}} : validateAction(action, m_payload.urls); !v.ok) {
        transitionTo(State::Error);
        setStatus(v.reason, u"error"_s);
        m_feedbackTimer.start(ErrorFeedbackMs);
        return false;
    }

    m_rejectTimer.stop();
    transitionTo(State::ActionConfirmed);
    m_pendingAction = action;
    setActiveIndex(index);
    qCInfo(lcState) << "confirmed" << actionId(action) << "for" << m_payload.urls.size() << "item(s)";

    const QUrl first = m_payload.urls.constFirst();
    const QUrl sourceFolder = first.isLocalFile() ? QUrl::fromLocalFile(QFileInfo(first.toLocalFile()).absolutePath())
                                                  : first.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);

    if (action == Action::Zip || action == Action::Extract) {
        // Next to the source by default; ask only when that folder is not writable.
        if (dryRun() || validateDestination(action, m_payload.urls, sourceFolder).ok) {
            startOperation(sourceFolder);
        } else {
            setStatus(tr("Choose where to save"), u"info"_s);
            Q_EMIT destinationRequested(int(action), QUrl::fromLocalFile(QDir::homePath()));
        }
        return true;
    }

    if (m_settings->destinationMode() == u"fixed"_s && m_settings->fixedDestination().isValid()) {
        startOperation(m_settings->fixedDestination());
    } else {
        setStatus(tr("Choose a destination"), u"info"_s);
        Q_EMIT destinationRequested(int(action), sourceFolder);
    }
    return true;
}

void DonutController::dismiss()
{
    switch (m_state) {
    case State::OperationRunning:
        if (m_service->isBusy()) {
            m_service->cancel();
        }
        break;
    case State::ActionConfirmed:
        cancelDestination();
        break;
    case State::MenuOpening:
    case State::MenuActive:
        if (m_persistent) {
            setPayloadUrls({});
            clearHover();
        } else {
            cancelMenu(tr("Cancelled"));
        }
        break;
    case State::PotentialDrag:
        transitionTo(State::Idle);
        break;
    case State::Completed:
    case State::Cancelled:
    case State::Error:
        m_feedbackTimer.stop();
        finishFeedback();
        break;
    case State::Idle:
        break;
    }
}

// --- Destination / operation ---------------------------------------------

void DonutController::provideDestination(const QUrl &folder)
{
    if (m_state != State::ActionConfirmed || !m_pendingAction) {
        return;
    }
    startOperation(folder);
}

void DonutController::cancelDestination()
{
    if (m_state != State::ActionConfirmed) {
        return;
    }
    m_pendingAction.reset();
    transitionTo(State::Cancelled);
    setStatus(tr("Cancelled"), u"info"_s);
    m_feedbackTimer.start(CancelFeedbackMs);
}

void DonutController::startOperation(const QUrl &destination)
{
    const Action action = *m_pendingAction;
    if (const Validation v = dryRun() ? Validation{true, {}} : validateDestination(action, m_payload.urls, destination); !v.ok) {
        transitionTo(State::Error);
        setStatus(v.reason, u"error"_s);
        m_feedbackTimer.start(ErrorFeedbackMs);
        return;
    }
    transitionTo(State::OperationRunning);
    setStatus(tr("Working…"), u"info"_s);

    if (dryRun()) {
        const QString where = destination.toDisplayString(QUrl::PreferLocalFile);
        const OperationResult simulated =
            OperationResult::success(action, tr("Dry run: would %1 %n item(s) into %2", nullptr, itemCount()).arg(actionLabel(action).toLower(), where));
        QMetaObject::invokeMethod(this, [this, simulated] { onOperationFinished(simulated); }, Qt::QueuedConnection);
        return;
    }
    m_service->start({action, m_payload.urls, destination});
}

void DonutController::onOperationFinished(const OperationResult &result)
{
    if (m_state != State::OperationRunning) {
        return;
    }
    m_pendingAction.reset();
    switch (result.status) {
    case OperationResult::Status::Success:
        transitionTo(State::Completed);
        setStatus(result.message, u"success"_s);
        m_feedbackTimer.start(SuccessFeedbackMs);
        break;
    case OperationResult::Status::Cancelled:
        transitionTo(State::Cancelled);
        setStatus(result.message, u"info"_s);
        m_feedbackTimer.start(CancelFeedbackMs);
        break;
    case OperationResult::Status::Failed:
        transitionTo(State::Error);
        setStatus(result.message, u"error"_s);
        m_feedbackTimer.start(ErrorFeedbackMs);
        break;
    }
    Q_EMIT operationFinished(result);
}

void DonutController::finishFeedback()
{
    if (m_state != State::Completed && m_state != State::Cancelled && m_state != State::Error) {
        return;
    }
    transitionTo(State::Idle);
    if (m_persistent) {
        setPayloadUrls({});
        setStatus({}, {});
        open(m_anchor, m_interactive);
        notifyOpened();
        return;
    }
    closeMenu();
}

void DonutController::setStatus(const QString &message, const QString &kind)
{
    if (m_statusMessage != message || m_statusKind != kind) {
        m_statusMessage = message;
        m_statusKind = kind;
        Q_EMIT statusChanged();
    }
}

// --- Opening / closing -------------------------------------------------------

void DonutController::openManually(const QPointF &globalPosition, bool interactive, const QList<QUrl> &urls)
{
    if (m_state != State::Idle && m_state != State::PotentialDrag) {
        qCDebug(lcState) << "manual activation ignored in state" << stateName(m_state);
        return;
    }
    setPayloadUrls(urls);
    open(globalPosition, interactive);
}

void DonutController::open(const QPointF &globalPosition, bool interactive)
{
    m_anchor = globalPosition;
    m_dragDropped = false;
    m_payloadRejected = false;
    setActiveIndex(HubIndex);
    if (!transitionTo(State::MenuOpening)) {
        return;
    }
    m_interactive = interactive;
    m_menuOpen = true;
    Q_EMIT menuOpenChanged();
    Q_EMIT openRequested(globalPosition, interactive);
}

void DonutController::notifyOpened()
{
    if (m_state == State::MenuOpening) {
        transitionTo(State::MenuActive);
    }
}

void DonutController::closeMenu()
{
    if (m_menuOpen) {
        m_menuOpen = false;
        Q_EMIT menuOpenChanged();
    }
}

void DonutController::notifyClosed()
{
    // A new activation may have reopened the menu during the fade-out.
    if (!m_menuOpen) {
        setDragInside(false);
        Q_EMIT hidden();
    }
}

void DonutController::closeSilently()
{
    if (menuAcceptsInput()) {
        transitionTo(State::Idle);
        setStatus({}, {});
        closeMenu();
    }
}

void DonutController::cancelMenu(const QString &message)
{
    if (!menuAcceptsInput()) {
        return;
    }
    transitionTo(State::Cancelled);
    setStatus(message, u"info"_s);
    m_feedbackTimer.start(CancelFeedbackMs);
}

// --- Backend events ------------------------------------------------------------

void DonutController::onDragStarted(const DragPayload &payload)
{
    if (m_state != State::Idle || !m_settings->enabled()) {
        return;
    }
    m_payload = payload;
    m_payloadRejected = false;
    recomputeActions();
    Q_EMIT payloadChanged();
    transitionTo(State::PotentialDrag);
}

void DonutController::onActivationRequested(const QPointF &position)
{
    if (!m_settings->enabled()) {
        return;
    }
    if (m_state != State::Idle && m_state != State::PotentialDrag) {
        // One menu per drag: the effect only activates once, but be defensive.
        qCDebug(lcState) << "activation ignored in state" << stateName(m_state);
        return;
    }
    open(position, false);
}

void DonutController::onDragFinished(bool dropped)
{
    if (m_state == State::PotentialDrag) {
        transitionTo(State::Idle);
        return;
    }
    if (menuAcceptsInput() && !m_persistent && !m_interactive) {
        m_dragDropped = dropped;
        m_dragEndGrace.start();
    }
}

void DonutController::onDragEndGrace()
{
    if (!menuAcceptsInput()) {
        return; // the drop reached us and an action was confirmed
    }
    // Released over Cancel or the hub: the drop was refused, the source got
    // "cancelled" and the pointer left our surface right at release time.
    const bool releasedOnDonut = !m_dragDropped && (m_dragInside || (m_sinceExit.isValid() && m_sinceExit.elapsed() < ReleaseAfterExitMs + DragEndGraceMs));
    if (releasedOnDonut && m_payload.hasUrls()) {
        cancelMenu(tr("Cancelled"));
    } else {
        closeSilently();
    }
}
}
