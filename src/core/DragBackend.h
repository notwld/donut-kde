// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "DragPayload.h"

#include <QObject>
#include <QPointF>

namespace Donut
{
/*
 * Source of drag lifecycle events for the controller.
 *
 * This is NOT a promise that Qt (or Wayland) exposes system-wide drags to
 * applications. Each implementation documents what it really observes; see
 * docs/architecture.md for the capability matrix.
 */
class DragBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ backendName CONSTANT)
    Q_PROPERTY(QString modeDescription READ modeDescription NOTIFY availabilityChanged)
    Q_PROPERTY(bool available READ isAvailable NOTIFY availabilityChanged)

public:
    enum class Capability {
        None = 0,
        GlobalDragLifecycle = 1 << 0, ///< sees drags started by any Wayland client
        PayloadBeforeEnter = 1 << 1, ///< knows MIME types/URLs before the drag reaches Donut
        CursorPosition = 1 << 2, ///< can report the global pointer position
        ManualActivation = 1 << 3,
        DryRun = 1 << 4, ///< actions are simulated, files are never touched
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    using QObject::QObject;
    ~DragBackend() override = default;

    virtual bool isAvailable() const = 0;
    virtual QString backendName() const = 0;
    /// One honest sentence for the UI and --diagnostics.
    virtual QString modeDescription() const = 0;
    virtual Capabilities capabilities() const = 0;
    virtual void start()
    {
    }
    /// Global pointer position if the backend can know it (CursorPosition).
    /// The callback is invoked asynchronously, with std::nullopt on failure.
    virtual void requestCursorPosition(std::function<void(std::optional<QPointF>)> callback);

    bool hasCapability(Capability c) const
    {
        return capabilities().testFlag(c);
    }

Q_SIGNALS:
    void availabilityChanged();
    /// A drag began. payload.payloadKnown may be false (KWin backend).
    void dragStarted(const Donut::DragPayload &payload);
    /// The backend decided the menu should open at this global position.
    void activationRequested(const QPointF &globalPosition);
    void dragMoved(const QPointF &globalPosition);
    /// The drag ended. dropped == true if some client accepted the drop.
    void dragFinished(bool dropped);
    void backendError(const QString &message);
};
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Donut::DragBackend::Capabilities)
