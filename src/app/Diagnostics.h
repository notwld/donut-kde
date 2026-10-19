// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>

namespace Donut
{
class SettingsManager;

/// Plain-text report for `donut --diagnostics`. Uses blocking D-Bus calls,
/// so it is only meant for the command line, never from the overlay.
QString diagnosticsReport(const SettingsManager &settings);
}
