// SPDX-License-Identifier: GPL-2.0-or-later
#include "DragPayload.h"

#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace Donut
{
bool DragPayload::isEmpty() const
{
    return urls.isEmpty();
}

bool DragPayload::hasUrls() const
{
    return !urls.isEmpty();
}

bool DragPayload::allLocalFiles() const
{
    if (urls.isEmpty()) {
        return false;
    }
    for (const QUrl &u : urls) {
        if (!u.isLocalFile()) {
            return false;
        }
    }
    return true;
}

bool DragPayload::anyDocumentPortalPath() const
{
    for (const QUrl &u : urls) {
        if (u.isLocalFile() && isDocumentPortalPath(u.toLocalFile())) {
            return true;
        }
    }
    return false;
}

QString DragPayload::displayName() const
{
    if (urls.isEmpty()) {
        return {};
    }
    const QUrl &first = urls.constFirst();
    QString name = first.fileName();
    if (name.isEmpty()) {
        name = first.toDisplayString(QUrl::PreferLocalFile | QUrl::StripTrailingSlash);
        name = name.section(u'/', -1);
    }
    return name.isEmpty() ? first.toDisplayString() : name;
}

QList<QUrl> parseUriList(const QByteArray &data)
{
    QList<QUrl> result;
    const QList<QByteArray> lines = data.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed(); // drops the '\r' of CRLF as well
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        const QUrl url = QUrl::fromEncoded(line, QUrl::StrictMode);
        if (url.isValid() && !url.scheme().isEmpty()) {
            result.append(url);
        }
    }
    return result;
}

bool isDocumentPortalPath(const QString &localPath)
{
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (!runtime.isEmpty() && localPath.startsWith(runtime + u"/doc/"_s)) {
        return true;
    }
    // Fallback when XDG_RUNTIME_DIR is unusual: /run/user/<uid>/doc/<id>/...
    return localPath.startsWith(u"/run/user/"_s) && localPath.section(u'/', 4, 4) == u"doc"_s;
}
}
