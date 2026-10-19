// SPDX-License-Identifier: GPL-2.0-or-later
#include "FileNaming.h"

#include <QDir>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace Donut::FileNaming
{
std::pair<QString, QString> splitName(const QString &fileName)
{
    static const QStringList doubleSuffixes = {u".tar.gz"_s, u".tar.xz"_s, u".tar.bz2"_s, u".tar.zst"_s};
    for (const QString &s : doubleSuffixes) {
        if (fileName.size() > s.size() && fileName.endsWith(s, Qt::CaseInsensitive)) {
            return {fileName.left(fileName.size() - s.size()), fileName.right(s.size())};
        }
    }
    const qsizetype dot = fileName.lastIndexOf(u'.');
    if (dot <= 0) { // no dot, or a leading-dot hidden file
        return {fileName, QString()};
    }
    return {fileName.left(dot), fileName.mid(dot)};
}

bool pathOccupied(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() || info.isSymLink();
}

QString uniqueChildPath(const QString &directory, const QString &fileName)
{
    const QDir dir(directory);
    QString candidate = dir.filePath(fileName);
    if (!pathOccupied(candidate)) {
        return candidate;
    }
    const auto [base, suffix] = splitName(fileName);
    for (int i = 1; i < 10000; ++i) {
        candidate = dir.filePath(u"%1 (%2)%3"_s.arg(base).arg(i).arg(suffix));
        if (!pathOccupied(candidate)) {
            return candidate;
        }
    }
    return {};
}

QString defaultArchiveName(const QList<QUrl> &sources)
{
    if (sources.size() == 1) {
        QString name = sources.constFirst().adjusted(QUrl::StripTrailingSlash).fileName();
        if (!name.isEmpty()) {
            if (!QFileInfo(sources.constFirst().toLocalFile()).isDir()) {
                name = splitName(name).first;
            }
            return name + u".zip"_s;
        }
    }
    return QObject::tr("Archive") + u".zip"_s;
}

QString extractionFolderName(const QString &archivePath)
{
    const QString name = splitName(QFileInfo(archivePath).fileName()).first;
    return name.isEmpty() ? QObject::tr("Extracted") : name;
}
}
