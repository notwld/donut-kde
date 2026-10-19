// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QPointF>

/*
 * Decides when an in-progress drag should open the donut.
 *
 * Pure logic with no KWin or GUI dependencies: it is compiled into both the
 * KWin effect (which feeds it real SeatInterface drag motion) and the unit
 * tests. Time is passed in explicitly so tests are deterministic.
 */
namespace Donut
{
class ActivationTracker
{
public:
    enum class Mode {
        Dwell,
        Distance,
        Manual, ///< never activates automatically
    };

    struct Config {
        Mode mode = Mode::Dwell;
        double threshold = 16.0; ///< logical px from drag start before anything can happen
        int dwellMs = 450; ///< how long the pointer must rest (Dwell mode)
        double dwellTolerance = 6.0; ///< movement allowed while resting
    };

    enum class Result {
        None,
        ArmDwellTimer, ///< caller must (re)start a single-shot timer of dwellRemainingMs()
        Activate, ///< open the menu at activationPosition(); returned at most once per drag
    };

    void setConfig(const Config &config);
    const Config &config() const;

    void begin(const QPointF &position, qint64 timeMs);
    Result motion(const QPointF &position, qint64 timeMs);
    /// Call when the dwell timer fires. Timers may fire early; in that case
    /// ArmDwellTimer is returned again so the caller waits for the remainder.
    Result dwellTimeout(qint64 timeMs);
    qint64 dwellRemainingMs(qint64 timeMs) const;
    void reset();

    bool isTracking() const;
    bool hasActivated() const;
    bool thresholdReached() const;
    QPointF startPosition() const;
    QPointF activationPosition() const;

private:
    Result activate(const QPointF &position);

    Config m_config;
    bool m_tracking = false;
    bool m_thresholdReached = false;
    bool m_activated = false;
    QPointF m_start;
    QPointF m_anchor;
    qint64 m_anchorTime = 0;
    QPointF m_activationPosition;
};
}
