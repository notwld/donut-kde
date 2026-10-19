// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QList>
#include <QString>

#include <optional>

/*
 * Validation of untrusted ZIP archives.
 *
 * Donut does not rely on KArchive's (lenient, normalising) view of entry
 * names to decide whether an archive is safe. It reads the raw central
 * directory itself and rejects the whole archive before anything is written
 * when any entry:
 *   - is absolute ("/etc/passwd", "C:\x"), contains "..", or a NUL byte
 *   - is a symlink whose target is absolute or contains ".."
 *   - duplicates another entry's path
 *   - pushes the declared uncompressed total or entry count over the limits
 */
namespace Donut::ArchiveValidation
{
struct PathCheck {
    bool ok = false;
    QString relativePath; ///< normalised, '/' separated; empty for the root entry
    QString error;
};

PathCheck validateEntryPath(const QString &entryName);
bool isSafeSymlinkTarget(const QString &target, QString *error = nullptr);

struct RawEntry {
    QString name;
    qint64 uncompressedSize = 0;
    bool isDirectory = false;
    bool isSymlink = false;
};

struct CentralDirectory {
    QList<RawEntry> entries;
    qint64 totalUncompressed = 0;
};

/// Parses the ZIP end-of-central-directory record (incl. ZIP64) and the central
/// directory. Returns std::nullopt with *error set when the file is not a
/// well-formed ZIP.
std::optional<CentralDirectory> readCentralDirectory(const QString &zipPath, QString *error);

struct Limits {
    qint64 maxEntries = 200000;
    qint64 maxTotalBytes = qint64(64) * 1024 * 1024 * 1024; ///< hard cap, also checked against free space
};

struct ArchiveCheck {
    bool ok = false;
    QString error;
    CentralDirectory directory;
};

/// Full pre-extraction validation of the raw archive. Symlink targets are not
/// stored in the central directory, so they are validated again during
/// extraction with isSafeSymlinkTarget().
ArchiveCheck validateArchive(const QString &zipPath, const Limits &limits = {});

/// True if the file content looks like a ZIP archive (by MIME magic, not extension).
bool isZipArchive(const QString &path);
}
