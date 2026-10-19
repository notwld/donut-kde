// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

// Small pill under the donut for status, success and error messages.
Rectangle {
    id: toast

    property string message
    property string kind // "", "info", "success", "error"
    property real animFactor: 1

    readonly property bool shown: message.length > 0 && kind.length > 0
    readonly property color dot: kind === "success" ? Theme.success : kind === "error" ? Theme.error : Theme.accent

    implicitWidth: Math.min(row.implicitWidth + 28, 360)
    implicitHeight: 34
    radius: height / 2
    color: Theme.withAlpha(Theme.background, 0.92)
    border.color: Theme.withAlpha(dot, 0.6)
    border.width: 1
    opacity: shown ? 1 : 0
    y: shown ? 0 : -6
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: 160 * toast.animFactor } }
    Behavior on y { NumberAnimation { duration: 160 * toast.animFactor; easing.type: Easing.OutCubic } }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: message

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 8
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 8; height: 8; radius: 4
            color: toast.dot
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, 300)
            text: toast.message
            color: Theme.text
            font.pixelSize: 12
            elide: Text.ElideMiddle
        }
    }
}
