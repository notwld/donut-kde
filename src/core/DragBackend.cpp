// SPDX-License-Identifier: GPL-2.0-or-later
#include "DragBackend.h"

#include <QTimer>

namespace Donut
{
void DragBackend::requestCursorPosition(std::function<void(std::optional<QPointF>)> callback)
{
    QTimer::singleShot(0, this, [callback = std::move(callback)] {
        callback(std::nullopt);
    });
}
}
