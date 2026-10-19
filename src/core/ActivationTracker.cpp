// SPDX-License-Identifier: GPL-2.0-or-later
#include "ActivationTracker.h"

#include <QLineF>

namespace Donut
{
void ActivationTracker::setConfig(const Config &config)
{
    m_config = config;
    if (m_config.threshold < 0.0) {
        m_config.threshold = 0.0;
    }
    if (m_config.dwellMs < 0) {
        m_config.dwellMs = 0;
    }
}

const ActivationTracker::Config &ActivationTracker::config() const
{
    return m_config;
}

void ActivationTracker::begin(const QPointF &position, qint64 timeMs)
{
    m_tracking = true;
    m_thresholdReached = false;
    m_activated = false;
    m_start = position;
    m_anchor = position;
    m_anchorTime = timeMs;
    m_activationPosition = {};
}

ActivationTracker::Result ActivationTracker::motion(const QPointF &position, qint64 timeMs)
{
    if (!m_tracking || m_activated || m_config.mode == Mode::Manual) {
        return Result::None;
    }

    if (!m_thresholdReached) {
        if (QLineF(m_start, position).length() < m_config.threshold) {
            return Result::None;
        }
        m_thresholdReached = true;
        if (m_config.mode == Mode::Distance) {
            return activate(position);
        }
        m_anchor = position;
        m_anchorTime = timeMs;
        return Result::ArmDwellTimer;
    }

    // Dwell mode: any real movement restarts the rest period.
    if (QLineF(m_anchor, position).length() > m_config.dwellTolerance) {
        m_anchor = position;
        m_anchorTime = timeMs;
        return Result::ArmDwellTimer;
    }
    return Result::None;
}

ActivationTracker::Result ActivationTracker::dwellTimeout(qint64 timeMs)
{
    if (!m_tracking || m_activated || !m_thresholdReached || m_config.mode != Mode::Dwell) {
        return Result::None;
    }
    // Timers can fire a little early; never activate before the pointer really
    // rested, but ask to be woken again instead of silently giving up.
    if (dwellRemainingMs(timeMs) > 0) {
        return Result::ArmDwellTimer;
    }
    return activate(m_anchor);
}

qint64 ActivationTracker::dwellRemainingMs(qint64 timeMs) const
{
    return std::max<qint64>(0, m_anchorTime + m_config.dwellMs - timeMs);
}

void ActivationTracker::reset()
{
    m_tracking = false;
    m_thresholdReached = false;
    m_activated = false;
}

bool ActivationTracker::isTracking() const
{
    return m_tracking;
}

bool ActivationTracker::hasActivated() const
{
    return m_activated;
}

bool ActivationTracker::thresholdReached() const
{
    return m_thresholdReached;
}

QPointF ActivationTracker::startPosition() const
{
    return m_start;
}

QPointF ActivationTracker::activationPosition() const
{
    return m_activationPosition;
}

ActivationTracker::Result ActivationTracker::activate(const QPointF &position)
{
    m_activated = true;
    m_activationPosition = position;
    return Result::Activate;
}
}
