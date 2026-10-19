// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QLoggingCategory>

// Enable everything with:  QT_LOGGING_RULES="donut.*.debug=true" donut
// or turn on "Diagnostic logging" in the settings page.
Q_DECLARE_LOGGING_CATEGORY(lcCore)
Q_DECLARE_LOGGING_CATEGORY(lcState)
Q_DECLARE_LOGGING_CATEGORY(lcBackend)
Q_DECLARE_LOGGING_CATEGORY(lcActions)
Q_DECLARE_LOGGING_CATEGORY(lcArchive)
Q_DECLARE_LOGGING_CATEGORY(lcUi)

namespace Donut
{
/// Applies or removes "donut.*.debug=true" at runtime.
void setDiagnosticLogging(bool enabled);
}
