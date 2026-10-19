// SPDX-License-Identifier: GPL-2.0-or-later
#include "Logging.h"

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcCore, "donut.core", QtInfoMsg)
Q_LOGGING_CATEGORY(lcState, "donut.state", QtInfoMsg)
Q_LOGGING_CATEGORY(lcBackend, "donut.backend", QtInfoMsg)
Q_LOGGING_CATEGORY(lcActions, "donut.actions", QtInfoMsg)
Q_LOGGING_CATEGORY(lcArchive, "donut.archive", QtInfoMsg)
Q_LOGGING_CATEGORY(lcUi, "donut.ui", QtInfoMsg)

namespace Donut
{
void setDiagnosticLogging(bool enabled)
{
    // Rules from QT_LOGGING_RULES still take precedence over filter rules set here.
    QLoggingCategory::setFilterRules(enabled ? u"donut.*.debug=true"_s : u"donut.*.debug=false"_s);
}
}
