// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "ArchiveValidation.h"
#include "OperationResult.h"

#include <QStringList>

/*
 * Blocking archive workers. FileActionService runs them on a thread pool;
 * tests call them directly. They never overwrite existing files:
 *  - createZip writes "<dir>/.<name>.donut-part" and renames it to a free name.
 *  - extractZip validates the whole archive first, extracts into a hidden
 *    temporary folder next to the destination and renames it into place only
 *    when everything succeeded, so failures and cancellation leave nothing behind.
 */
namespace Donut::ArchiveOperations
{
OperationResult createZip(const QStringList &sourcePaths, const QString &targetDirectory, const QString &archiveName, OperationControl *control);

OperationResult extractZip(const QString &archivePath, const QString &targetDirectory, OperationControl *control, const ArchiveValidation::Limits &limits = {});
}
