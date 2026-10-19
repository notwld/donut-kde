// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

// Theme icon (Breeze on Plasma) recoloured to a token colour by the C++
// image provider, with bundled SVG fallbacks.
Image {
    id: icon

    property string name
    property color tint: Theme.text
    property bool keepColors: false
    property real size: 32

    width: size
    height: size
    sourceSize.width: Math.ceil(size * Screen.devicePixelRatio)
    sourceSize.height: Math.ceil(size * Screen.devicePixelRatio)
    fillMode: Image.PreserveAspectFit
    smooth: true
    asynchronous: false
    cache: true
    source: name.length === 0 ? "" : keepColors ? "image://donuticon/" + name : "image://donuticon/" + name + "@" + tint.toString()
}
