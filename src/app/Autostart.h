// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>

namespace Donut::Autostart
{
/// ~/.config/autostart/org.kde.donut.desktop
QString entryPath();
bool isEnabled();
/// Writes or removes only Donut's own autostart entry.
bool setEnabled(bool enabled, const QString &executable);
}
