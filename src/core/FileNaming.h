// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QUrl>

namespace Donut::FileNaming
{
/// Splits "photo.tar.gz" into {"photo", ".tar.gz"} and ".bashrc" into {".bashrc", ""}.
std::pair<QString, QString> splitName(const QString &fileName);

/// Returns dir/fileName if free, otherwise dir/"name (1).ext", "name (2).ext", ...
/// Uses lstat semantics: a dangling symlink still occupies the name.
QString uniqueChildPath(const QString &directory, const QString &fileName);

/// "Holiday.zip" for a single item, "Archive.zip" for several.
QString defaultArchiveName(const QList<QUrl> &sources);

/// Folder name used when extracting "photos.zip" -> "photos".
QString extractionFolderName(const QString &archivePath);

bool pathOccupied(const QString &path);
}
