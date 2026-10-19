// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include <optional>

namespace Donut
{
Q_NAMESPACE

/// The actions offered by the ring. The numeric order is the default
/// clockwise order starting at 12 o'clock.
enum class Action {
    Copy = 0,
    Move,
    Zip,
    Extract,
    Cancel,
};
Q_ENUM_NS(Action)

/// Controller state machine. See docs/architecture.md for the transition table.
enum class State {
    Idle,
    PotentialDrag,
    MenuOpening,
    MenuActive,
    ActionConfirmed,
    OperationRunning,
    Completed,
    Cancelled,
    Error,
};
Q_ENUM_NS(State)

/// How the compositor-side tracker decides to open the menu during a drag.
enum class TriggerMode {
    Dwell, ///< move past the threshold, then pause (default, avoids ordinary drags)
    Distance, ///< open as soon as the threshold is crossed (opt-in: every drag)
    ShortcutOnly, ///< never open automatically
};
Q_ENUM_NS(TriggerMode)

QString actionId(Action action);
QString actionLabel(Action action);
QString actionIconName(Action action);
std::optional<Action> actionFromId(QStringView id);
QList<Action> allActions();

QString stateName(State state);

QString triggerModeId(TriggerMode mode);
TriggerMode triggerModeFromId(QStringView id, TriggerMode fallback = TriggerMode::Dwell);
}
