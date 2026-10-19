// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "DragBackend.h"

namespace Donut
{
/*
 * Fallback that works on any compositor: Donut shows a normal window and the
 * user drags files onto it. Nothing outside Donut's window is observed; the
 * drag lifecycle is whatever Qt delivers to that window.
 */
class DropTargetBackend : public DragBackend
{
    Q_OBJECT

public:
    using DragBackend::DragBackend;

    bool isAvailable() const override
    {
        return true;
    }
    QString backendName() const override
    {
        return QStringLiteral("drop-target");
    }
    QString modeDescription() const override
    {
        return tr("Drop-target fallback: drag files onto the Donut window. No desktop-wide drag detection.");
    }
    Capabilities capabilities() const override
    {
        return Capability::ManualActivation;
    }
};
}
