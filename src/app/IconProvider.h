// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QQuickImageProvider>

namespace Donut
{
/*
 * image://donuticon/<theme-icon-name>[@#rrggbb]
 *
 * Looks the icon up in the user's icon theme (Breeze on Plasma) and falls
 * back to Donut's bundled SVGs. With a colour suffix the glyph is recoloured
 * so action icons always contrast with the dark glass, whatever the theme.
 */
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};
}
