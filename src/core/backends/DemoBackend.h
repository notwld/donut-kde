// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "DragBackend.h"

namespace Donut
{
/*
 * Manual demo mode for trying the UI. The menu is opened by a button or
 * shortcut, the payload is a description only and every action is a dry run:
 * no file is read, written or deleted.
 */
class DemoBackend : public DragBackend
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
        return QStringLiteral("demo");
    }
    QString modeDescription() const override
    {
        return tr("Manual demo mode: activated by hand, actions are simulated (dry run). No drag detection.");
    }
    Capabilities capabilities() const override
    {
        return Capability::ManualActivation | Capability::DryRun;
    }
};
}
