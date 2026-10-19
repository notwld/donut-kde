// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QList>
#include <QMetaType>
#include <QPointF>
#include <QStringList>
#include <QUrl>

namespace Donut
{
/*
 * What Donut knows about the thing being dragged.
 *
 * With the KWin backend the compositor cannot tell us the payload (see
 * docs/wayland-limitations.md), so a payload starts out with payloadKnown ==
 * false and is filled in when the drag enters Donut's own surface and the
 * Wayland data offer becomes readable.
 */
struct DragPayload {
    QList<QUrl> urls;
    QStringList mimeTypes;
    QString mimeType; ///< preferred type, "text/uri-list" for file drags
    QPointF position; ///< global logical coordinates, if known
    bool isExternalDrag = false; ///< originated in another application
    bool payloadKnown = false;

    bool isEmpty() const;
    bool hasUrls() const;
    bool allLocalFiles() const;
    bool anyDocumentPortalPath() const;
    QString displayName() const; ///< first item's file name, for the hub
};

/// Parses text/uri-list (RFC 2483): CRLF or LF separated, '#' comment lines.
/// Invalid lines are dropped. Never interprets the data as anything but URLs.
QList<QUrl> parseUriList(const QByteArray &data);

/// True for paths served by xdg-document-portal (/run/user/UID/doc/...).
/// Such paths are portal copies exported by sandboxed apps, not the originals.
bool isDocumentPortalPath(const QString &localPath);
}

Q_DECLARE_METATYPE(Donut::DragPayload)
