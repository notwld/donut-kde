// SPDX-License-Identifier: GPL-2.0-or-later
pragma Singleton

import QtQuick

// Design tokens for the donut. Translucency is applied where used so the
// glass reads well on both light and dark wallpapers.
QtObject {
    readonly property color background: "#0B1220"
    readonly property color surface: "#111C2E"
    readonly property color segment: "#203653"
    readonly property color accent: "#168BFF"
    readonly property color activeAccent: "#45B4FF"
    readonly property color text: "#F4F8FF"
    readonly property color secondaryText: "#A9BDD6"
    readonly property color success: "#35D39A"
    readonly property color error: "#FF5D73"

    readonly property real glassOpacity: 0.82
    readonly property real segmentOpacity: 0.72

    function withAlpha(c, a) {
        return Qt.rgba(c.r, c.g, c.b, a)
    }

    function mix(a, b, t) {
        return Qt.rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t)
    }
}
