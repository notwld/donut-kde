// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Shapes
import org.kde.donut.core

// The central hub: payload icon and name, or the hovered action's label,
// progress while an operation runs, then success/error feedback.
Item {
    id: hub

    required property DonutController controller
    property real radius: 50
    property int selectMs: 130

    width: radius * 2
    height: radius * 2

    readonly property int state_: controller.state
    readonly property bool running: state_ === Donut.OperationRunning || state_ === Donut.ActionConfirmed
    readonly property bool succeeded: state_ === Donut.Completed
    readonly property bool failed: state_ === Donut.Error
    readonly property var activeAction: controller.activeIndex >= 0 ? controller.actions[controller.activeIndex] : null

    readonly property color ringColor: succeeded ? Theme.success : failed ? Theme.error : Theme.accent

    readonly property string primaryText: {
        if (succeeded || failed || state_ === Donut.Cancelled)
            return controller.statusMessage
        if (running)
            return controller.statusMessage
        if (controller.payloadRejected)
            return qsTr("Not a file")
        if (activeAction)
            return activeAction.label
        if (!controller.payloadKnown)
            return qsTr("Drop on an action")
        if (!controller.hasPayload)
            return controller.interactive ? qsTr("Drop files here") : ""
        return controller.payloadName
    }
    readonly property string secondaryText: {
        if (running || succeeded || failed || activeAction || !controller.hasPayload)
            return ""
        return controller.itemCount > 1 ? qsTr("+%1 more").arg(controller.itemCount - 1) : ""
    }
    readonly property string iconName: succeeded ? "dialog-ok-apply"
                                     : failed ? "dialog-error"
                                     : activeAction ? activeAction.icon
                                     : controller.hasPayload ? controller.payloadIcon : "go-down"

    Accessible.role: Accessible.StaticText
    Accessible.name: primaryText

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.withAlpha(Qt.lighter(Theme.surface, 1.35), 0.95) }
            GradientStop { position: 1; color: Theme.withAlpha(Theme.background, 0.95) }
        }
        border.width: 1.5
        border.color: Theme.withAlpha(hub.ringColor, hub.activeAction || hub.succeeded || hub.failed ? 0.95 : 0.5)
        Behavior on border.color { ColorAnimation { duration: hub.selectMs } }
    }

    // Progress ring: determinate when the service reports progress, spinning otherwise.
    Shape {
        id: progressRing
        anchors.fill: parent
        visible: hub.running
        preferredRendererType: Shape.CurveRenderer
        readonly property bool indeterminate: hub.controller.progress < 0
        ShapePath {
            fillColor: "transparent"
            strokeColor: Theme.activeAccent
            strokeWidth: 3
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: hub.radius; centerY: hub.radius
                radiusX: hub.radius - 4; radiusY: hub.radius - 4
                startAngle: -90
                sweepAngle: progressRing.indeterminate ? 90 : Math.max(2, 360 * hub.controller.progress)
            }
        }
        RotationAnimator on rotation {
            running: progressRing.visible && progressRing.indeterminate
            from: 0; to: 360; duration: 900; loops: Animation.Infinite
        }
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 18
        spacing: 1

        ActionIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: hub.iconName
            size: hub.radius * 0.62
            keepColors: hub.controller.hasPayload && !hub.activeAction && !hub.succeeded && !hub.failed
            tint: hub.succeeded ? Theme.success : hub.failed ? Theme.error : Theme.text
            Behavior on size { NumberAnimation { duration: hub.selectMs } }
        }

        FadingText {
            width: parent.width
            text: hub.primaryText
            color: Theme.text
            pixelSize: hub.radius > 46 ? 12 : 11
            bold: hub.activeAction !== null
            duration: hub.selectMs
        }
        Text {
            width: parent.width
            visible: text.length > 0
            text: hub.secondaryText
            color: Theme.secondaryText
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // Brief pulse on success/error.
    SequentialAnimation on scale {
        running: hub.succeeded || hub.failed
        NumberAnimation { to: 1.08; duration: 110; easing.type: Easing.OutCubic }
        NumberAnimation { to: 1.0; duration: 160; easing.type: Easing.InOutCubic }
    }

    component FadingText: Text {
        id: ft
        property int duration: 120
        property real pixelSize: 12
        property bool bold: false
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideMiddle
        maximumLineCount: 2
        wrapMode: Text.Wrap
        font.pixelSize: pixelSize
        font.weight: bold ? Font.DemiBold : Font.Normal
        onTextChanged: swap.restart()
        // Dip and recover so label changes read as a transition, not a jump.
        SequentialAnimation {
            id: swap
            NumberAnimation { target: ft; property: "opacity"; to: 0.15; duration: ft.duration * 0.4 }
            NumberAnimation { target: ft; property: "opacity"; to: 1; duration: ft.duration * 0.6 }
        }
    }
}
