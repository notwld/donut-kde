// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "DonutTypes.h"

#include <QList>
#include <QString>
#include <QUrl>

namespace Donut
{
enum class ItemKind {
    RegularFile,
    Directory,
    Symlink,
    Special, ///< socket, FIFO, device
    Missing,
    Remote, ///< not a local file; only KIO-capable actions can handle it
};

/// Classifies without following symlinks.
ItemKind classifyUrl(const QUrl &url);

struct Validation {
    bool ok = false;
    QString reason; ///< short, user-visible explanation when !ok
};

/// Is the action applicable to these items at all? Used to dim segments.
Validation validateAction(Action action, const QList<QUrl> &sources);

/// Can the action write into this destination folder? (Copy, Move, Zip, Extract)
Validation validateDestination(Action action, const QList<QUrl> &sources, const QUrl &destinationFolder);
}
