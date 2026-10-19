// SPDX-License-Identifier: GPL-2.0-or-later
#include "ActionValidator.h"
#include "ArchiveValidation.h"
#include "DragPayload.h"

#include <KProtocolInfo>

#include <QDir>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace Donut
{
ItemKind classifyUrl(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return ItemKind::Remote;
    }
    const QFileInfo info(url.toLocalFile());
    if (info.isSymLink()) {
        return ItemKind::Symlink;
    }
    if (!info.exists()) {
        return ItemKind::Missing;
    }
    if (info.isDir()) {
        return ItemKind::Directory;
    }
    if (info.isFile()) {
        return ItemKind::RegularFile;
    }
    return ItemKind::Special;
}

namespace
{
Validation fail(const QString &reason)
{
    return {false, reason};
}

QString canonicalOrClean(const QString &path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(path) : canonical;
}
}

Validation validateAction(Action action, const QList<QUrl> &sources)
{
    if (action == Action::Cancel) {
        return {true, {}};
    }
    if (sources.isEmpty()) {
        return fail(QObject::tr("Nothing selected"));
    }

    for (const QUrl &url : sources) {
        if (!url.isValid()) {
            return fail(QObject::tr("Invalid item"));
        }
        const ItemKind kind = classifyUrl(url);
        if (kind == ItemKind::Missing) {
            return fail(QObject::tr("\"%1\" does not exist").arg(url.fileName()));
        }
        if (kind == ItemKind::Remote) {
            if (action == Action::Zip || action == Action::Extract) {
                return fail(QObject::tr("Only local files can be archived"));
            }
            if (!KProtocolInfo::isKnownProtocol(url)) {
                return fail(QObject::tr("Unsupported location \"%1\"").arg(url.scheme()));
            }
            continue;
        }
        const QString path = url.toLocalFile();
        if (action == Action::Move) {
            if (isDocumentPortalPath(path)) {
                // A sandboxed app exported a portal copy; moving it would not move the original.
                return fail(QObject::tr("Cannot move items shared by a sandboxed app"));
            }
            const QString clean = QDir::cleanPath(path);
            if (clean == u"/"_s || clean == QDir::cleanPath(QDir::homePath())) {
                return fail(QObject::tr("Refusing to move this folder"));
            }
        }
        if (action == Action::Extract) {
            if (kind != ItemKind::RegularFile) {
                return fail(QObject::tr("Extract needs a ZIP archive"));
            }
            if (!ArchiveValidation::isZipArchive(path)) {
                return fail(QObject::tr("\"%1\" is not a ZIP archive").arg(url.fileName()));
            }
        }
    }
    return {true, {}};
}

Validation validateDestination(Action action, const QList<QUrl> &sources, const QUrl &destinationFolder)
{
    if (action == Action::Cancel) {
        return {true, {}};
    }
    if (!destinationFolder.isValid() || destinationFolder.isEmpty()) {
        return fail(QObject::tr("No destination chosen"));
    }
    if (!destinationFolder.isLocalFile()) {
        if ((action == Action::Copy || action == Action::Move) && KProtocolInfo::isKnownProtocol(destinationFolder)) {
            return {true, {}};
        }
        return fail(QObject::tr("Destination must be a local folder"));
    }

    const QFileInfo dest(destinationFolder.toLocalFile());
    if (!dest.isDir()) {
        return fail(QObject::tr("Destination is not a folder"));
    }
    if (!dest.isWritable()) {
        return fail(QObject::tr("Destination folder is read-only"));
    }

    const QString destPath = canonicalOrClean(dest.absoluteFilePath());
    for (const QUrl &url : sources) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QFileInfo src(url.toLocalFile());
        const QString srcPath = src.isSymLink() ? QDir::cleanPath(src.absoluteFilePath()) : canonicalOrClean(src.absoluteFilePath());
        if (src.isDir() && !src.isSymLink() && (destPath == srcPath || destPath.startsWith(srcPath + u'/'))) {
            return fail(QObject::tr("Cannot put a folder inside itself"));
        }
        if (action == Action::Move && canonicalOrClean(src.absolutePath()) == destPath) {
            return fail(QObject::tr("\"%1\" is already in that folder").arg(src.fileName()));
        }
    }
    return {true, {}};
}
}
