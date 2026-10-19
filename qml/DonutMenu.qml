// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Shapes
import QtQuick.Effects
import org.kde.donut.core

// The radial menu itself. All hit testing goes through the C++ controller so
// the geometry matches the unit-tested RadialGeometry exactly.
Item {
    id: menu

    required property DonutController controller
    required property SettingsManager settings
    property bool opened: controller.menuOpen
    property real glowMargin: 28

    signal openFinished()
    signal closeFinished()
    signal filesRequested()

    readonly property real outerRadius: controller.outerRadius
    readonly property real innerRadius: controller.innerRadius
    readonly property real animFactor: settings.effectiveAnimationFactor
    readonly property int openMs: Math.round(settings.openDurationMs * animFactor)
    readonly property int closeMs: Math.round(130 * animFactor)
    readonly property int selectMs: Math.round(130 * animFactor)
    readonly property real cx: width / 2
    readonly property real cy: height / 2
    readonly property int state_: controller.state
    readonly property bool feedback: state_ === Donut.Completed || state_ === Donut.Error || state_ === Donut.Cancelled

    width: 2 * (outerRadius + glowMargin)
    height: width
    focus: controller.interactive
    opacity: 0
    scale: 0.82

    Accessible.role: Accessible.PopupMenu
    Accessible.name: qsTr("Donut actions")

    states: State {
        name: "open"
        when: menu.opened
        PropertyChanges { menu.opacity: 1; menu.scale: 1 }
    }
    transitions: [
        Transition {
            to: "open"
            SequentialAnimation {
                ParallelAnimation {
                    NumberAnimation { property: "opacity"; duration: menu.openMs * 0.75; easing.type: Easing.OutCubic }
                    NumberAnimation { property: "scale"; duration: menu.openMs; easing.type: Easing.OutBack; easing.overshoot: 1.4 }
                }
                ScriptAction { script: menu.openFinished() }
            }
        },
        Transition {
            from: "open"
            SequentialAnimation {
                ParallelAnimation {
                    NumberAnimation { property: "opacity"; duration: menu.closeMs; easing.type: Easing.InCubic }
                    NumberAnimation { property: "scale"; to: 0.9; duration: menu.closeMs; easing.type: Easing.InCubic }
                }
                ScriptAction { script: menu.closeFinished() }
            }
        }
    ]

    // --- Glass ring with a soft outer glow ------------------------------------
    Shape {
        id: ring
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        readonly property color edge: menu.state_ === Donut.Completed ? Theme.success
                                    : menu.state_ === Donut.Error ? Theme.error : Theme.accent
        layer.enabled: menu.settings.glowIntensity > 0
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: ring.edge
            shadowBlur: 1.0
            shadowOpacity: 0.55 * menu.settings.glowIntensity
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 0
            autoPaddingEnabled: false
        }
        ShapePath {
            fillColor: Theme.withAlpha(Theme.surface, Theme.glassOpacity)
            strokeColor: Theme.withAlpha(ring.edge, 0.55)
            strokeWidth: 1.5
            fillRule: ShapePath.OddEvenFill
            PathAngleArc { centerX: menu.cx; centerY: menu.cy; radiusX: menu.outerRadius + 3; radiusY: menu.outerRadius + 3; startAngle: 0; sweepAngle: 360 }
            PathAngleArc { centerX: menu.cx; centerY: menu.cy; radiusX: menu.innerRadius + 2; radiusY: menu.innerRadius + 2; startAngle: 0; sweepAngle: 360 }
        }
    }

    // --- Segments ---------------------------------------------------------------
    Item {
        anchors.fill: parent
        opacity: menu.controller.payloadRejected ? 0.4 : 1
        Behavior on opacity { NumberAnimation { duration: menu.selectMs } }
        Repeater {
            model: menu.controller.actions
            delegate: DonutSegment {
                anchors.fill: parent
                count: menu.controller.sectorCount
                innerRadius: menu.innerRadius
                outerRadius: menu.outerRadius
                gapDegrees: menu.settings.sectorSpacing
                active: menu.controller.activeIndex === index
                showLabels: menu.settings.showLabels
                glowIntensity: menu.settings.glowIntensity
                selectMs: menu.selectMs
                dimmed: menu.feedback
            }
        }
    }

    DonutCenter {
        anchors.centerIn: parent
        controller: menu.controller
        radius: menu.innerRadius
        selectMs: menu.selectMs
    }

    // --- Drag and drop (Wayland data device, delivered by Qt) -----------------
    DropArea {
        id: dropArea
        anchors.fill: parent

        function evaluate(drag) {
            const dx = drag.x - menu.cx
            const dy = drag.y - menu.cy
            menu.controller.hoverAt(dx, dy)
            if (menu.controller.canDropAt(dx, dy))
                drag.accept(Qt.CopyAction) // never Move: the source must not delete anything
            else
                drag.accepted = false
        }

        onEntered: drag => {
            // Read text/uri-list explicitly; see DonutController::dragEntered.
            const uris = drag.formats.indexOf("text/uri-list") >= 0 ? drag.getDataAsString("text/uri-list") : ""
            const wanted = menu.controller.dragEntered(drag.formats, uris)
            menu.controller.hoverAt(drag.x - menu.cx, drag.y - menu.cy)
            // Qt Quick stops delivering a drag to a DropArea that refused the
            // enter event, so accept any file drag here (the pointer usually
            // starts over the hub) and refuse per position in onPositionChanged.
            if (wanted)
                drag.accept(Qt.CopyAction)
            else
                drag.accepted = false
        }
        onPositionChanged: drag => evaluate(drag)
        onExited: menu.controller.dragExited()
        onDropped: drop => {
            const uris = drop.formats.indexOf("text/uri-list") >= 0 ? drop.getDataAsString("text/uri-list") : ""
            if (menu.controller.dropAt(drop.x - menu.cx, drop.y - menu.cy, uris))
                drop.accept(Qt.CopyAction)
            else
                drop.accepted = false
        }
    }

    // --- Pointer (manual / keyboard-accessible mode) ---------------------------
    HoverHandler {
        id: hover
        enabled: menu.controller.interactive
        onPointChanged: if (hovered) menu.controller.hoverAt(point.position.x - menu.cx, point.position.y - menu.cy)
        onHoveredChanged: if (!hovered) menu.controller.clearHover()
    }
    TapHandler {
        enabled: menu.controller.interactive
        onTapped: eventPoint => {
            const idx = menu.controller.sectorAt(eventPoint.position.x - menu.cx, eventPoint.position.y - menu.cy)
            if (idx >= 0)
                menu.controller.confirmAction(idx)
            else if (idx === -1 && !menu.controller.hasPayload)
                menu.filesRequested()
            else if (idx === -2 && !menu.controller.persistent)
                menu.controller.dismiss()
        }
    }

    Keys.onPressed: event => {
        switch (event.key) {
        case Qt.Key_Right:
        case Qt.Key_Down:
        case Qt.Key_Tab:
            menu.controller.selectNext(); break
        case Qt.Key_Left:
        case Qt.Key_Up:
        case Qt.Key_Backtab:
            menu.controller.selectPrevious(); break
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
            if (menu.controller.activeIndex >= 0)
                menu.controller.confirmActive()
            else if (!menu.controller.hasPayload)
                menu.filesRequested()
            break
        case Qt.Key_Escape:
            menu.controller.dismiss(); break
        default:
            if (event.key >= Qt.Key_1 && event.key <= Qt.Key_9 && event.key - Qt.Key_1 < menu.controller.sectorCount) {
                menu.controller.hoverAt(0, 0) // reset, then select by number
                for (let i = 0; i <= event.key - Qt.Key_1; ++i) menu.controller.selectNext()
                break
            }
            return
        }
        event.accepted = true
    }
}
