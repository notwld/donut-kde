// SPDX-License-Identifier: GPL-2.0-or-later
#include "IconProvider.h"

#include <QFile>
#include <QIcon>
#include <QPainter>

using namespace Qt::StringLiterals;

namespace Donut
{
IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const qsizetype at = id.indexOf(u"@#"_s);
    const QString name = at < 0 ? id : id.left(at);
    const QColor tint = at < 0 ? QColor() : QColor(id.mid(at + 1));
    const QSize target = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(48, 48);

    QIcon icon = QIcon::fromTheme(name);
    if (icon.isNull()) {
        const QString bundled = u":/icons/%1.svg"_s.arg(name);
        icon = QFile::exists(bundled) ? QIcon(bundled) : QIcon::fromTheme(u"unknown"_s);
    }
    QPixmap pixmap = icon.pixmap(target);
    if (tint.isValid() && !pixmap.isNull()) {
        QPainter p(&pixmap);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pixmap.rect(), tint);
    }
    if (size) {
        *size = pixmap.size();
    }
    return pixmap;
}
}
