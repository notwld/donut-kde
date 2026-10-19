// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Shapes
import QtQuick.Effects

// One annular sector. Angles follow RadialGeometry.h: 0° is 12 o'clock,
// clockwise. Qt's PathAngleArc measures from 3 o'clock, hence the -90.
Item {
    id: seg

    required property int index
    required property var modelData

    property int count: 5
    property real innerRadius: 50
    property real outerRadius: 170
    property real gapDegrees: 2
    property bool active: false
    property bool showLabels: true
    property real glowIntensity: 0.7
    property int selectMs: 130
    property bool dimmed: false

    readonly property bool actionEnabled: modelData.enabled
    readonly property real sweep: 360 / Math.max(1, count)
    readonly property real centerAngle: index * sweep
    readonly property real arcStart: centerAngle - sweep / 2 + gapDegrees / 2
    readonly property real arcSweep: Math.max(1, sweep - gapDegrees)
    readonly property real cx: width / 2
    readonly property real cy: height / 2
    readonly property real rIn: innerRadius + 5

    // 0 → 1 while this segment is the active one; drives every highlight.
    property real lift: active && actionEnabled ? 1 : 0
    Behavior on lift {
        NumberAnimation { duration: seg.selectMs; easing.type: Easing.OutCubic }
    }

    readonly property real rOut: outerRadius + lift * 5

    function px(r, deg) { return cx + r * Math.cos((deg - 90) * Math.PI / 180) }
    function py(r, deg) { return cy + r * Math.sin((deg - 90) * Math.PI / 180) }

    Accessible.role: Accessible.MenuItem
    Accessible.name: modelData.label
    Accessible.description: actionEnabled ? "" : modelData.reason

    component SectorPath: ShapePath {
        id: path
        property real outer: seg.rOut
        PathAngleArc {
            centerX: seg.cx; centerY: seg.cy
            radiusX: path.outer; radiusY: path.outer
            startAngle: seg.arcStart - 90
            sweepAngle: seg.arcSweep
        }
        PathAngleArc {
            centerX: seg.cx; centerY: seg.cy
            radiusX: seg.rIn; radiusY: seg.rIn
            startAngle: seg.arcStart + seg.arcSweep - 90
            sweepAngle: -seg.arcSweep
            moveToStart: false
        }
        PathLine { x: seg.px(path.outer, seg.arcStart); y: seg.py(path.outer, seg.arcStart) }
    }

    // Glow: a blurred copy of the active sector. The layer only exists while lit.
    Shape {
        id: glow
        anchors.fill: parent
        visible: seg.lift > 0.01 && seg.glowIntensity > 0
        opacity: seg.lift * seg.glowIntensity
        preferredRendererType: Shape.CurveRenderer
        layer.enabled: visible
        layer.effect: MultiEffect {
            blurEnabled: true
            blur: 1.0
            blurMax: 32
            brightness: 0.15
        }
        SectorPath {
            fillColor: Theme.activeAccent
            strokeColor: "transparent"
        }
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        SectorPath {
            fillColor: seg.actionEnabled
                       ? Theme.mix(Theme.withAlpha(Theme.segment, Theme.segmentOpacity), Theme.withAlpha(Theme.accent, 0.62), seg.lift)
                       : Theme.withAlpha(Theme.segment, 0.38)
            strokeColor: Theme.mix(Theme.withAlpha(Theme.accent, 0.38), Theme.activeAccent, seg.lift)
            strokeWidth: 1.2 + seg.lift
        }
    }

    Column {
        id: content
        readonly property real midR: (seg.rIn + seg.rOut) / 2 + seg.lift * 2
        x: seg.px(midR, seg.centerAngle) - width / 2
        y: seg.py(midR, seg.centerAngle) - height / 2
        spacing: 2
        opacity: seg.actionEnabled ? (seg.dimmed ? 0.6 : 1) : 0.35

        ActionIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: seg.modelData.icon
            size: 28 + seg.lift * 6
            tint: seg.lift > 0.5 ? "#FFFFFF" : Theme.text
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: seg.showLabels
            text: seg.modelData.label
            color: Theme.mix(Theme.secondaryText, Theme.text, seg.lift)
            font.pixelSize: 13
            font.weight: seg.lift > 0.5 ? Font.DemiBold : Font.Medium
            font.letterSpacing: 0.3
        }
    }
}
