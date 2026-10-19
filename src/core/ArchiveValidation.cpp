// SPDX-License-Identifier: GPL-2.0-or-later
#include "ArchiveValidation.h"
#include "Logging.h"

#include <QFile>
#include <QMimeDatabase>
#include <QSet>
#include <QtEndian>

#include <sys/stat.h>

using namespace Qt::StringLiterals;

namespace Donut::ArchiveValidation
{
namespace
{
constexpr quint32 EocdSignature = 0x06054b50;
constexpr quint32 Zip64EocdSignature = 0x06064b50;
constexpr quint32 Zip64LocatorSignature = 0x07064b50;
constexpr quint32 CentralHeaderSignature = 0x02014b50;
constexpr qint64 EocdSize = 22;
constexpr qint64 CentralHeaderSize = 46;
constexpr qint64 MaxCommentSize = 0xFFFF;
constexpr qint64 MaxCentralDirectorySize = qint64(256) * 1024 * 1024;

quint16 u16(const QByteArray &b, qint64 off)
{
    return qFromLittleEndian<quint16>(b.constData() + off);
}
quint32 u32(const QByteArray &b, qint64 off)
{
    return qFromLittleEndian<quint32>(b.constData() + off);
}
quint64 u64(const QByteArray &b, qint64 off)
{
    return qFromLittleEndian<quint64>(b.constData() + off);
}

QByteArray readAt(QFile &f, qint64 offset, qint64 size)
{
    if (offset < 0 || size < 0 || offset + size > f.size() || !f.seek(offset)) {
        return {};
    }
    QByteArray data = f.read(size);
    return data.size() == size ? data : QByteArray();
}
}

PathCheck validateEntryPath(const QString &entryName)
{
    PathCheck result;
    if (entryName.contains(QChar(u'\0'))) {
        result.error = QObject::tr("entry name contains a NUL byte");
        return result;
    }
    QString name = entryName;
    // Archives made on Windows sometimes use backslashes; treat them as
    // separators so "..\\evil" cannot slip through.
    name.replace(u'\\', u'/');
    if (name.startsWith(u'/')) {
        result.error = QObject::tr("absolute path \"%1\"").arg(entryName);
        return result;
    }
    if (name.size() >= 2 && name.at(1) == u':' && name.at(0).isLetter()) {
        result.error = QObject::tr("drive-letter path \"%1\"").arg(entryName);
        return result;
    }
    QStringList clean;
    for (const QString &part : name.split(u'/', Qt::SkipEmptyParts)) {
        if (part == u"."_s) {
            continue;
        }
        if (part == u".."_s) {
            result.error = QObject::tr("path traversal in \"%1\"").arg(entryName);
            return result;
        }
        clean.append(part);
    }
    result.ok = true;
    result.relativePath = clean.join(u'/');
    return result;
}

bool isSafeSymlinkTarget(const QString &target, QString *error)
{
    auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return false;
    };
    if (target.isEmpty()) {
        return fail(QObject::tr("empty symlink target"));
    }
    if (target.contains(QChar(u'\0')) || target.contains(u'\\')) {
        return fail(QObject::tr("symlink target contains forbidden characters"));
    }
    if (target.startsWith(u'/')) {
        return fail(QObject::tr("symlink points to an absolute path"));
    }
    // Only purely downward relative links are allowed. Lexical checks of ".."
    // are not enough because the kernel resolves ".." after following other
    // links, so any ".." is rejected outright.
    for (const QString &part : target.split(u'/', Qt::SkipEmptyParts)) {
        if (part == u".."_s) {
            return fail(QObject::tr("symlink escapes the extraction folder"));
        }
    }
    return true;
}

std::optional<CentralDirectory> readCentralDirectory(const QString &zipPath, QString *error)
{
    auto fail = [error](const QString &message) -> std::optional<CentralDirectory> {
        if (error) {
            *error = message;
        }
        return std::nullopt;
    };

    QFile f(zipPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return fail(QObject::tr("cannot open archive: %1").arg(f.errorString()));
    }
    const qint64 fileSize = f.size();
    if (fileSize < EocdSize) {
        return fail(QObject::tr("not a ZIP archive (file too small)"));
    }

    // Locate the end of central directory record, scanning back over a possible comment.
    const qint64 tailSize = std::min(fileSize, EocdSize + MaxCommentSize);
    const QByteArray tail = readAt(f, fileSize - tailSize, tailSize);
    if (tail.size() != tailSize) {
        return fail(QObject::tr("cannot read archive"));
    }
    qint64 eocdInTail = -1;
    for (qint64 i = tail.size() - EocdSize; i >= 0; --i) {
        if (u32(tail, i) == EocdSignature) {
            eocdInTail = i;
            break;
        }
    }
    if (eocdInTail < 0) {
        return fail(QObject::tr("not a ZIP archive (no end of central directory)"));
    }
    const qint64 eocdOffset = fileSize - tailSize + eocdInTail;
    if (u16(tail, eocdInTail + 4) != 0 || u16(tail, eocdInTail + 6) != 0) {
        return fail(QObject::tr("multi-disk ZIP archives are not supported"));
    }
    quint64 entryCount = u16(tail, eocdInTail + 10);
    quint64 cdSize = u32(tail, eocdInTail + 12);
    quint64 cdOffset = u32(tail, eocdInTail + 16);

    if (entryCount == 0xFFFF || cdSize == 0xFFFFFFFF || cdOffset == 0xFFFFFFFF) {
        const QByteArray locator = readAt(f, eocdOffset - 20, 20);
        if (locator.isEmpty() || u32(locator, 0) != Zip64LocatorSignature) {
            return fail(QObject::tr("corrupt ZIP64 archive (missing locator)"));
        }
        const QByteArray z64 = readAt(f, qint64(u64(locator, 8)), 56);
        if (z64.isEmpty() || u32(z64, 0) != Zip64EocdSignature) {
            return fail(QObject::tr("corrupt ZIP64 archive (bad end record)"));
        }
        entryCount = u64(z64, 32);
        cdSize = u64(z64, 40);
        cdOffset = u64(z64, 48);
    }

    if (cdSize > quint64(MaxCentralDirectorySize) || cdOffset + cdSize > quint64(fileSize)) {
        return fail(QObject::tr("corrupt ZIP archive (central directory out of range)"));
    }
    const QByteArray cd = readAt(f, qint64(cdOffset), qint64(cdSize));
    if (cd.size() != qint64(cdSize)) {
        return fail(QObject::tr("corrupt ZIP archive (truncated central directory)"));
    }

    CentralDirectory dir;
    qint64 pos = 0;
    for (quint64 i = 0; i < entryCount; ++i) {
        if (pos + CentralHeaderSize > cd.size() || u32(cd, pos) != CentralHeaderSignature) {
            return fail(QObject::tr("corrupt ZIP archive (bad central directory entry)"));
        }
        const quint16 madeBy = u16(cd, pos + 4);
        const quint16 flags = u16(cd, pos + 8);
        quint64 uncompressed = u32(cd, pos + 24);
        const quint16 nameLen = u16(cd, pos + 28);
        const quint16 extraLen = u16(cd, pos + 30);
        const quint16 commentLen = u16(cd, pos + 32);
        const quint32 externalAttr = u32(cd, pos + 38);
        const qint64 nameOff = pos + CentralHeaderSize;
        if (nameOff + nameLen + extraLen + commentLen > cd.size()) {
            return fail(QObject::tr("corrupt ZIP archive (entry exceeds central directory)"));
        }
        const QByteArray rawName = cd.mid(nameOff, nameLen);

        if (uncompressed == 0xFFFFFFFF) {
            // ZIP64 extended information extra field (id 0x0001) starts with the uncompressed size.
            qint64 e = nameOff + nameLen;
            const qint64 end = e + extraLen;
            while (e + 4 <= end) {
                const quint16 id = u16(cd, e);
                const quint16 len = u16(cd, e + 2);
                if (id == 0x0001 && len >= 8 && e + 4 + 8 <= end) {
                    uncompressed = u64(cd, e + 4);
                    break;
                }
                e += 4 + len;
            }
        }

        RawEntry entry;
        // Bit 11 = UTF-8 names. Others are CP437 in theory; Latin-1 keeps every
        // byte (including '/', '\\' and '.') intact, which is what validation needs.
        entry.name = (flags & 0x0800) ? QString::fromUtf8(rawName) : QString::fromLatin1(rawName);
        entry.uncompressedSize = qint64(std::min<quint64>(uncompressed, quint64(std::numeric_limits<qint64>::max())));
        const bool unixAttrs = (madeBy >> 8) == 3;
        const quint32 mode = externalAttr >> 16;
        entry.isSymlink = unixAttrs && S_ISLNK(mode);
        entry.isDirectory = rawName.endsWith('/') || (unixAttrs && S_ISDIR(mode));
        dir.totalUncompressed += entry.uncompressedSize;
        dir.entries.append(entry);

        pos = nameOff + nameLen + extraLen + commentLen;
    }
    return dir;
}

ArchiveCheck validateArchive(const QString &zipPath, const Limits &limits)
{
    ArchiveCheck check;
    QString error;
    auto dir = readCentralDirectory(zipPath, &error);
    if (!dir) {
        check.error = error;
        qCInfo(lcArchive) << "archive rejected:" << error;
        return check;
    }
    if (dir->entries.size() > limits.maxEntries) {
        check.error = QObject::tr("archive has too many entries (%1)").arg(dir->entries.size());
        return check;
    }
    if (dir->totalUncompressed > limits.maxTotalBytes) {
        check.error = QObject::tr("archive expands to more than the allowed size");
        return check;
    }

    QSet<QString> files;
    QSet<QString> directories;
    for (const RawEntry &e : std::as_const(dir->entries)) {
        const PathCheck p = validateEntryPath(e.name);
        if (!p.ok) {
            check.error = p.error;
            qCInfo(lcArchive) << "archive rejected: unsafe entry";
            return check;
        }
        if (p.relativePath.isEmpty()) {
            continue;
        }
        if (e.isDirectory) {
            if (files.contains(p.relativePath)) {
                check.error = QObject::tr("archive contains \"%1\" as both a file and a folder").arg(p.relativePath);
                return check;
            }
            directories.insert(p.relativePath);
            continue;
        }
        if (files.contains(p.relativePath) || directories.contains(p.relativePath)) {
            check.error = QObject::tr("archive contains duplicate entry \"%1\"").arg(p.relativePath);
            return check;
        }
        files.insert(p.relativePath);
    }
    check.ok = true;
    check.directory = std::move(*dir);
    return check;
}

bool isZipArchive(const QString &path)
{
    static const QMimeDatabase db;
    const QMimeType type = db.mimeTypeForFile(path, QMimeDatabase::MatchContent);
    return type.inherits(u"application/zip"_s) || type.name() == u"application/zip"_s;
}
}
