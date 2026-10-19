// SPDX-License-Identifier: GPL-2.0-or-later
#include "ArchiveOperations.h"
#include "FileNaming.h"
#include "Logging.h"

#include <KArchiveDirectory>
#include <KArchiveFile>
#include <KZip>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStorageInfo>
#include <QTemporaryDir>

#include <memory>
#include <unistd.h>

using namespace Qt::StringLiterals;

namespace Donut::ArchiveOperations
{
namespace
{
constexpr qint64 ChunkSize = 256 * 1024;

mode_t toMode(QFileDevice::Permissions p)
{
    mode_t m = 0;
    m |= (p & QFileDevice::ReadOwner) ? 0400 : 0;
    m |= (p & QFileDevice::WriteOwner) ? 0200 : 0;
    m |= (p & QFileDevice::ExeOwner) ? 0100 : 0;
    m |= (p & QFileDevice::ReadGroup) ? 0040 : 0;
    m |= (p & QFileDevice::WriteGroup) ? 0020 : 0;
    m |= (p & QFileDevice::ExeGroup) ? 0010 : 0;
    m |= (p & QFileDevice::ReadOther) ? 0004 : 0;
    m |= (p & QFileDevice::WriteOther) ? 0002 : 0;
    m |= (p & QFileDevice::ExeOther) ? 0001 : 0;
    return m;
}

QFileDevice::Permissions fromMode(mode_t m)
{
    QFileDevice::Permissions p;
    p.setFlag(QFileDevice::ReadOwner, m & 0400);
    p.setFlag(QFileDevice::ReadUser, m & 0400);
    p.setFlag(QFileDevice::WriteOwner, m & 0200);
    p.setFlag(QFileDevice::WriteUser, m & 0200);
    p.setFlag(QFileDevice::ExeOwner, m & 0100);
    p.setFlag(QFileDevice::ExeUser, m & 0100);
    p.setFlag(QFileDevice::ReadGroup, m & 0040);
    p.setFlag(QFileDevice::WriteGroup, m & 0020);
    p.setFlag(QFileDevice::ExeGroup, m & 0010);
    p.setFlag(QFileDevice::ReadOther, m & 0004);
    p.setFlag(QFileDevice::WriteOther, m & 0002);
    p.setFlag(QFileDevice::ExeOther, m & 0001);
    return p;
}

/// Never restore setuid/setgid/sticky bits from an untrusted archive, and keep
/// extracted items manageable by their owner.
mode_t sanitizedMode(mode_t archived, bool directory)
{
    mode_t m = archived & 0777;
    if (m == 0) {
        m = directory ? 0755 : 0644;
    }
    return m | (directory ? 0700 : 0600);
}

bool isInside(const QString &root, const QString &path)
{
    return path == root || path.startsWith(root + u'/');
}

struct ZipItem {
    enum class Kind {
        File,
        Directory,
        Symlink,
    };
    Kind kind = Kind::File;
    QFileInfo info;
    QString archivePath;
};
}

OperationResult createZip(const QStringList &sourcePaths, const QString &targetDirectory, const QString &archiveName, OperationControl *control)
{
    constexpr Action A = Action::Zip;
    const QFileInfo dirInfo(targetDirectory);
    if (!dirInfo.isDir() || !dirInfo.isWritable()) {
        return OperationResult::failure(A, QObject::tr("Cannot write to \"%1\"").arg(targetDirectory));
    }
    if (sourcePaths.isEmpty()) {
        return OperationResult::failure(A, QObject::tr("Nothing to archive"));
    }

    const QString partPath = FileNaming::uniqueChildPath(targetDirectory, u"."_s + archiveName + u".donut-part"_s);
    if (partPath.isEmpty()) {
        return OperationResult::failure(A, QObject::tr("Could not find a free temporary file name"));
    }

    // Scan first so progress has a total and so we never archive our own output.
    QList<ZipItem> items;
    qint64 totalBytes = 0;
    int skipped = 0;
    QSet<QString> topLevelNames;

    std::function<bool(const QFileInfo &, const QString &)> collect = [&](const QFileInfo &info, const QString &archivePath) {
        if (control && control->isCancelled()) {
            return false;
        }
        if (info.absoluteFilePath() == partPath) {
            return true;
        }
        if (info.isSymLink()) {
            items.append({ZipItem::Kind::Symlink, info, archivePath});
        } else if (info.isDir()) {
            items.append({ZipItem::Kind::Directory, info, archivePath});
            const QFileInfoList children =
                QDir(info.absoluteFilePath()).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
            for (const QFileInfo &child : children) {
                if (!collect(child, archivePath + u'/' + child.fileName())) {
                    return false;
                }
            }
        } else if (info.isFile()) {
            items.append({ZipItem::Kind::File, info, archivePath});
            totalBytes += info.size();
        } else {
            ++skipped; // sockets, FIFOs, device nodes
        }
        return true;
    };

    for (const QString &source : sourcePaths) {
        const QFileInfo info(source);
        if (!info.exists() && !info.isSymLink()) {
            return OperationResult::failure(A, QObject::tr("\"%1\" no longer exists").arg(info.fileName()));
        }
        QString top = info.fileName();
        if (topLevelNames.contains(top)) {
            const auto [base, suffix] = FileNaming::splitName(top);
            for (int i = 1; topLevelNames.contains(top); ++i) {
                top = u"%1 (%2)%3"_s.arg(base).arg(i).arg(suffix);
            }
        }
        topLevelNames.insert(top);
        if (!collect(info, top)) {
            return OperationResult::cancelled(A);
        }
    }

    KZip zip(partPath);
    if (!zip.open(QIODevice::WriteOnly)) {
        return OperationResult::failure(A, QObject::tr("Cannot create archive: %1").arg(zip.errorString()));
    }
    auto abort = [&](OperationResult result) {
        zip.close();
        QFile::remove(partPath);
        return result;
    };

    qint64 done = 0;
    QByteArray buffer;
    for (const ZipItem &item : std::as_const(items)) {
        if (control && control->isCancelled()) {
            return abort(OperationResult::cancelled(A));
        }
        const QFileInfo &fi = item.info;
        const mode_t mode = toMode(fi.permissions());
        switch (item.kind) {
        case ZipItem::Kind::Directory:
            if (!zip.writeDir(item.archivePath, fi.owner(), fi.group(), 040000 | mode, fi.lastRead(), fi.lastModified(), fi.metadataChangeTime())) {
                return abort(OperationResult::failure(A, QObject::tr("Cannot add folder \"%1\"").arg(item.archivePath)));
            }
            break;
        case ZipItem::Kind::Symlink:
            // Store the link itself, never what it points to.
            if (!zip.writeSymLink(item.archivePath, fi.readSymLink(), fi.owner(), fi.group(), 0120777, fi.lastRead(), fi.lastModified(), fi.metadataChangeTime())) {
                return abort(OperationResult::failure(A, QObject::tr("Cannot add link \"%1\"").arg(item.archivePath)));
            }
            break;
        case ZipItem::Kind::File: {
            QFile in(fi.absoluteFilePath());
            if (!in.open(QIODevice::ReadOnly)) {
                return abort(OperationResult::failure(A, QObject::tr("Cannot read \"%1\": %2").arg(fi.fileName(), in.errorString())));
            }
            if (!zip.prepareWriting(item.archivePath, fi.owner(), fi.group(), fi.size(), 0100000 | mode, fi.lastRead(), fi.lastModified(), fi.metadataChangeTime())) {
                return abort(OperationResult::failure(A, QObject::tr("Cannot add \"%1\"").arg(item.archivePath)));
            }
            qint64 written = 0;
            while (!in.atEnd()) {
                if (control && control->isCancelled()) {
                    zip.finishWriting(written);
                    return abort(OperationResult::cancelled(A));
                }
                buffer = in.read(ChunkSize);
                if (buffer.isEmpty() && in.error() != QFileDevice::NoError) {
                    zip.finishWriting(written);
                    return abort(OperationResult::failure(A, QObject::tr("Read error in \"%1\"").arg(fi.fileName())));
                }
                if (!zip.writeData(buffer.constData(), buffer.size())) {
                    zip.finishWriting(written);
                    return abort(OperationResult::failure(A, QObject::tr("Write error (disk full?)")));
                }
                written += buffer.size();
                done += buffer.size();
                if (control) {
                    control->reportProgress(done, totalBytes);
                }
            }
            if (!zip.finishWriting(written)) {
                return abort(OperationResult::failure(A, QObject::tr("Cannot finish \"%1\"").arg(item.archivePath)));
            }
            break;
        }
        }
    }

    if (!zip.close()) {
        QFile::remove(partPath);
        return OperationResult::failure(A, QObject::tr("Cannot finish archive: %1").arg(zip.errorString()));
    }

    // QFile::rename refuses to replace an existing file, so loop on races.
    for (int attempt = 0; attempt < 5; ++attempt) {
        const QString finalPath = FileNaming::uniqueChildPath(targetDirectory, archiveName);
        if (!finalPath.isEmpty() && QFile::rename(partPath, finalPath)) {
            OperationResult r = OperationResult::success(A, QObject::tr("Created %1").arg(QFileInfo(finalPath).fileName()), {QUrl::fromLocalFile(finalPath)});
            r.skipped = skipped;
            qCInfo(lcArchive) << "zip created with" << items.size() << "entries," << skipped << "skipped";
            return r;
        }
    }
    QFile::remove(partPath);
    return OperationResult::failure(A, QObject::tr("Could not move the finished archive into place"));
}

OperationResult extractZip(const QString &archivePath, const QString &targetDirectory, OperationControl *control, const ArchiveValidation::Limits &limits)
{
    constexpr Action A = Action::Extract;
    const QFileInfo dirInfo(targetDirectory);
    if (!dirInfo.isDir() || !dirInfo.isWritable()) {
        return OperationResult::failure(A, QObject::tr("Cannot write to \"%1\"").arg(targetDirectory));
    }

    const ArchiveValidation::ArchiveCheck check = ArchiveValidation::validateArchive(archivePath, limits);
    if (!check.ok) {
        return OperationResult::failure(A, QObject::tr("Refusing to extract: %1").arg(check.error));
    }
    const QStorageInfo storage(targetDirectory);
    if (storage.isValid() && storage.bytesAvailable() >= 0 && check.directory.totalUncompressed > storage.bytesAvailable()) {
        return OperationResult::failure(A, QObject::tr("Not enough free space to extract this archive"));
    }

    KZip zip(archivePath);
    if (!zip.open(QIODevice::ReadOnly)) {
        return OperationResult::failure(A, QObject::tr("Cannot read archive: %1").arg(zip.errorString()));
    }

    // Build the plan from KArchive's tree, validating every path again.
    struct Planned {
        QString relativePath;
        const KArchiveEntry *entry;
    };
    QList<Planned> dirs;
    QList<Planned> files;
    QList<Planned> links;
    qint64 totalBytes = 0;
    QString planError;

    std::function<bool(const KArchiveDirectory *, const QString &)> plan = [&](const KArchiveDirectory *dir, const QString &prefix) {
        const QStringList names = dir->entries();
        for (const QString &name : names) {
            const KArchiveEntry *e = dir->entry(name);
            const QString rel = prefix.isEmpty() ? name : prefix + u'/' + name;
            const ArchiveValidation::PathCheck pc = ArchiveValidation::validateEntryPath(rel);
            if (!pc.ok || pc.relativePath.isEmpty()) {
                planError = pc.ok ? QObject::tr("empty entry name") : pc.error;
                return false;
            }
            if (!e->symLinkTarget().isEmpty()) {
                QString linkError;
                if (!ArchiveValidation::isSafeSymlinkTarget(e->symLinkTarget(), &linkError)) {
                    planError = u"%1: %2"_s.arg(pc.relativePath, linkError);
                    return false;
                }
                links.append({pc.relativePath, e});
            } else if (e->isDirectory()) {
                dirs.append({pc.relativePath, e});
                if (!plan(static_cast<const KArchiveDirectory *>(e), pc.relativePath)) {
                    return false;
                }
            } else if (e->isFile()) {
                files.append({pc.relativePath, e});
                totalBytes += static_cast<const KArchiveFile *>(e)->size();
            }
        }
        return true;
    };
    if (!plan(zip.directory(), QString())) {
        return OperationResult::failure(A, QObject::tr("Refusing to extract: %1").arg(planError));
    }

    QTemporaryDir staging(QDir(targetDirectory).filePath(u".donut-extract-XXXXXX"_s));
    if (!staging.isValid()) {
        return OperationResult::failure(A, QObject::tr("Cannot create a temporary folder: %1").arg(staging.errorString()));
    }
    const QString root = QFileInfo(staging.path()).canonicalFilePath();
    const QDir rootDir(root);

    auto ensureParent = [&](const QString &rel) {
        const QString parent = QFileInfo(rootDir.filePath(rel)).absolutePath();
        if (!QDir().mkpath(parent)) {
            return false;
        }
        // Defence in depth: the parent must resolve inside the staging folder.
        return isInside(root, QFileInfo(parent).canonicalFilePath());
    };

    for (const Planned &d : std::as_const(dirs)) {
        if (!QDir().mkpath(rootDir.filePath(d.relativePath))) {
            return OperationResult::failure(A, QObject::tr("Cannot create folder \"%1\"").arg(d.relativePath));
        }
    }

    qint64 done = 0;
    for (const Planned &p : std::as_const(files)) {
        if (control && control->isCancelled()) {
            return OperationResult::cancelled(A);
        }
        const auto *file = static_cast<const KArchiveFile *>(p.entry);
        if (!ensureParent(p.relativePath)) {
            return OperationResult::failure(A, QObject::tr("Unsafe destination for \"%1\"").arg(p.relativePath));
        }
        QFile out(rootDir.filePath(p.relativePath));
        // NewOnly = O_EXCL: never overwrite and never write through an existing symlink.
        if (!out.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            return OperationResult::failure(A, QObject::tr("Cannot create \"%1\": %2").arg(p.relativePath, out.errorString()));
        }
        std::unique_ptr<QIODevice> in(file->createDevice());
        if (!in || (!in->isOpen() && !in->open(QIODevice::ReadOnly))) {
            return OperationResult::failure(A, QObject::tr("Cannot read \"%1\" from the archive").arg(p.relativePath));
        }
        qint64 remaining = file->size();
        while (remaining > 0) {
            if (control && control->isCancelled()) {
                return OperationResult::cancelled(A);
            }
            const QByteArray chunk = in->read(std::min(ChunkSize, remaining));
            if (chunk.isEmpty()) {
                return OperationResult::failure(A, QObject::tr("\"%1\" is corrupt or truncated").arg(p.relativePath));
            }
            if (out.write(chunk) != chunk.size()) {
                return OperationResult::failure(A, QObject::tr("Write error (disk full?)"));
            }
            remaining -= chunk.size();
            done += chunk.size();
            if (control) {
                control->reportProgress(done, totalBytes);
            }
        }
        out.setPermissions(fromMode(sanitizedMode(file->permissions(), false)));
        if (file->date().isValid()) {
            out.setFileTime(file->date(), QFileDevice::FileModificationTime);
        }
        out.close();
    }

    // Symlinks last: while files are written no link exists that could redirect them.
    for (const Planned &l : std::as_const(links)) {
        if (!ensureParent(l.relativePath)) {
            return OperationResult::failure(A, QObject::tr("Unsafe destination for \"%1\"").arg(l.relativePath));
        }
        const QByteArray target = QFile::encodeName(l.entry->symLinkTarget());
        const QByteArray linkPath = QFile::encodeName(rootDir.filePath(l.relativePath));
        if (::symlink(target.constData(), linkPath.constData()) != 0) {
            return OperationResult::failure(A, QObject::tr("Cannot create link \"%1\"").arg(l.relativePath));
        }
    }

    // Directory permissions after their contents are in place.
    for (const Planned &d : std::as_const(dirs)) {
        QFile::setPermissions(rootDir.filePath(d.relativePath), fromMode(sanitizedMode(d.entry->permissions(), true)));
    }

    for (int attempt = 0; attempt < 5; ++attempt) {
        const QString finalPath = FileNaming::uniqueChildPath(targetDirectory, FileNaming::extractionFolderName(archivePath));
        // Qt uses renameat2(RENAME_NOREPLACE) on Linux, so this cannot clobber a folder created meanwhile.
        if (!finalPath.isEmpty() && QDir().rename(staging.path(), finalPath)) {
            staging.setAutoRemove(false);
            qCInfo(lcArchive) << "extracted" << files.size() << "files," << dirs.size() << "folders," << links.size() << "links";
            return OperationResult::success(A, QObject::tr("Extracted to %1").arg(QFileInfo(finalPath).fileName()), {QUrl::fromLocalFile(finalPath)});
        }
    }
    return OperationResult::failure(A, QObject::tr("Could not move the extracted folder into place"));
}
}
