// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "ActionValidator.h"
#include "DonutTypes.h"
#include "DragPayload.h"
#include "OperationResult.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

namespace Donut
{
class DragBackend;
class FileActionService;
class SettingsManager;

/*
 * Owns the menu state machine and everything QML needs to render it.
 *
 * Geometry arguments (dx, dy) are always relative to the donut centre in
 * logical pixels, y pointing down. See RadialGeometry.h for the angle
 * convention and docs/architecture.md for the state transition table.
 */
class DonutController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Donut::State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString stateName READ stateNameString NOTIFY stateChanged)
    Q_PROPERTY(bool menuOpen READ menuOpen NOTIFY menuOpenChanged)
    Q_PROPERTY(bool interactive READ interactive NOTIFY menuOpenChanged)
    Q_PROPERTY(bool persistent READ persistent CONSTANT)
    Q_PROPERTY(QVariantList actions READ actionsModel NOTIFY actionsChanged)
    Q_PROPERTY(int sectorCount READ sectorCount NOTIFY actionsChanged)
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeIndexChanged)
    Q_PROPERTY(bool hasPayload READ hasPayload NOTIFY payloadChanged)
    Q_PROPERTY(bool payloadKnown READ payloadKnown NOTIFY payloadChanged)
    Q_PROPERTY(bool payloadRejected READ payloadRejected NOTIFY payloadChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY payloadChanged)
    Q_PROPERTY(QString payloadName READ payloadName NOTIFY payloadChanged)
    Q_PROPERTY(QString payloadIcon READ payloadIcon NOTIFY payloadChanged)
    Q_PROPERTY(bool dragInside READ dragInside NOTIFY dragInsideChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString statusKind READ statusKind NOTIFY statusChanged)
    Q_PROPERTY(QString backendName READ backendName NOTIFY backendChanged)
    Q_PROPERTY(QString modeDescription READ modeDescription NOTIFY backendChanged)
    Q_PROPERTY(bool dryRun READ dryRun NOTIFY backendChanged)
    Q_PROPERTY(double outerRadius READ outerRadius NOTIFY geometryChanged)
    Q_PROPERTY(double innerRadius READ innerRadius NOTIFY geometryChanged)

public:
    static constexpr int HubIndex = -1;
    static constexpr int OutsideIndex = -2;

    DonutController(SettingsManager *settings, FileActionService *service, QObject *parent = nullptr);
    ~DonutController() override;

    void setBackend(DragBackend *backend);
    DragBackend *backend() const;

    /// Persistent controllers (the standalone window) never close the menu;
    /// after each operation they return to MenuActive.
    void setPersistent(bool persistent);

    State state() const;
    QString stateNameString() const;
    bool menuOpen() const;
    bool interactive() const;
    bool persistent() const;
    QVariantList actionsModel() const;
    int sectorCount() const;
    int activeIndex() const;
    Action actionAt(int index) const;
    bool hasPayload() const;
    bool payloadKnown() const;
    bool payloadRejected() const;
    int itemCount() const;
    QString payloadName() const;
    QString payloadIcon() const;
    const DragPayload &payload() const;
    bool dragInside() const;
    double progress() const;
    QString statusMessage() const;
    QString statusKind() const;
    QString backendName() const;
    QString modeDescription() const;
    bool dryRun() const;
    double outerRadius() const;
    double innerRadius() const;
    QPointF anchorPosition() const;

    // --- Geometry / hover ------------------------------------------------
    Q_INVOKABLE int sectorAt(qreal dx, qreal dy) const;
    Q_INVOKABLE void hoverAt(qreal dx, qreal dy);
    Q_INVOKABLE void clearHover();
    Q_INVOKABLE bool isActionEnabled(int index) const;

    // --- Drag-and-drop onto the donut (Wayland data device via Qt) --------
    /// Returns true if Donut wants this drag (file URLs present).
    Q_INVOKABLE bool dragEntered(const QStringList &formats, const QString &uriList);
    Q_INVOKABLE void dragExited();
    Q_INVOKABLE bool canDropAt(qreal dx, qreal dy) const;
    /// Returns true if the drop was accepted and an action confirmed.
    Q_INVOKABLE bool dropAt(qreal dx, qreal dy, const QString &uriList);

    // --- Keyboard / click ------------------------------------------------
    Q_INVOKABLE void selectNext();
    Q_INVOKABLE void selectPrevious();
    Q_INVOKABLE bool confirmActive();
    Q_INVOKABLE bool confirmAction(int index);
    /// Escape: closes the menu, abandons a pending destination or cancels a running operation.
    Q_INVOKABLE void dismiss();

    // --- Lifecycle callbacks from QML -------------------------------------
    Q_INVOKABLE void notifyOpened();
    Q_INVOKABLE void notifyClosed();
    Q_INVOKABLE void provideDestination(const QUrl &folder);
    Q_INVOKABLE void cancelDestination();
    Q_INVOKABLE void setPayloadUrls(const QList<QUrl> &urls);

    /// Shortcut, CLI, demo and window modes. urls may be empty (drop files later).
    void openManually(const QPointF &globalPosition, bool interactive, const QList<QUrl> &urls = {});

    /// Exposed for tests: the allowed-transition table.
    static bool isTransitionAllowed(State from, State to);

Q_SIGNALS:
    void stateChanged();
    void menuOpenChanged();
    void actionsChanged();
    void activeIndexChanged();
    void payloadChanged();
    void dragInsideChanged();
    void progressChanged();
    void statusChanged();
    void backendChanged();
    void geometryChanged();

    /// The overlay should be placed at globalPosition and shown.
    void openRequested(const QPointF &globalPosition, bool interactive);
    /// The close animation finished; the overlay may be unmapped.
    void hidden();
    /// Copy/Move need a folder: QML shows a folder dialog and answers with
    /// provideDestination() or cancelDestination().
    void destinationRequested(int action, const QUrl &suggestedFolder);
    void operationFinished(const Donut::OperationResult &result);

private:
    bool transitionTo(State next);
    void open(const QPointF &globalPosition, bool interactive);
    void closeMenu();
    void closeSilently();
    void cancelMenu(const QString &message);
    void setPayload(const DragPayload &payload);
    void recomputeActions();
    void setActiveIndex(int index);
    void setStatus(const QString &message, const QString &kind);
    void setDragInside(bool inside);
    void startOperation(const QUrl &destination);
    void onOperationFinished(const OperationResult &result);
    void finishFeedback();
    void onDragStarted(const DragPayload &payload);
    void onActivationRequested(const QPointF &position);
    void onDragFinished(bool dropped);
    void onDragEndGrace();
    bool menuAcceptsInput() const;

    SettingsManager *m_settings;
    FileActionService *m_service;
    QPointer<DragBackend> m_backend;

    State m_state = State::Idle;
    bool m_menuOpen = false;
    bool m_interactive = false;
    bool m_persistent = false;
    QList<Action> m_ring;
    QList<Validation> m_validity;
    int m_activeIndex = HubIndex;
    DragPayload m_payload;
    bool m_payloadRejected = false;
    bool m_dragInside = false;
    QPointF m_anchor;
    std::optional<Action> m_pendingAction;
    QString m_statusMessage;
    QString m_statusKind;
    double m_progress = -1.0;

    // Drag-end bookkeeping (see onDragFinished)
    QTimer m_dragEndGrace;
    QTimer m_feedbackTimer;
    QTimer m_rejectTimer;
    bool m_dragDropped = false;
    QElapsedTimer m_sinceExit;
};
}
