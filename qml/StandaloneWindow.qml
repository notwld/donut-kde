// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.donut.core

// Regular window for the drop-target fallback (`donut --window`) and the
// dry-run demo (`donut --demo`). The banner always states what the mode can do.
Window {
    id: root

    required property DonutController controller
    required property SettingsManager settings
    property bool demo: false

    signal settingsRequested()

    title: demo ? qsTr("Donut — demo (dry run)") : qsTr("Donut — drop target")
    width: 560
    height: 660
    minimumWidth: 460
    minimumHeight: 560
    color: Theme.background

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.lighter(Theme.background, 1.25) }
            GradientStop { position: 1; color: Theme.background }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: banner.implicitHeight + 16
            radius: 8
            color: Theme.withAlpha(root.demo ? Theme.error : Theme.accent, 0.14)
            border.color: Theme.withAlpha(root.demo ? Theme.error : Theme.accent, 0.5)
            Text {
                id: banner
                anchors.fill: parent
                anchors.margins: 8
                wrapMode: Text.Wrap
                color: Theme.text
                font.pixelSize: 12
                text: root.controller.modeDescription
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            DonutMenu {
                id: menu
                anchors.centerIn: parent
                controller: root.controller
                settings: root.settings
                opened: true
                focus: true
                onFilesRequested: filePicker.open()
            }
            ToastNotification {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: menu.bottom
                anchors.topMargin: -12
                message: root.controller.statusMessage
                kind: root.controller.statusKind
                animFactor: root.settings.effectiveAnimationFactor
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: qsTr("Choose files…")
                icon.name: "document-open"
                onClicked: filePicker.open()
            }
            Button {
                visible: root.demo
                text: qsTr("Simulate a drag")
                icon.name: "transform-move"
                onClicked: root.controller.setPayloadUrls([
                    "file:///home/demo/Pictures/Holiday%202026.jpg",
                    "file:///home/demo/Documents/notes.txt"
                ])
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Settings")
                icon.name: "configure"
                onClicked: root.settingsRequested()
            }
        }
        Text {
            Layout.fillWidth: true
            color: Theme.secondaryText
            font.pixelSize: 11
            wrapMode: Text.Wrap
            text: qsTr("Drag files onto a segment, or choose files and use the arrow keys and Enter. Esc clears the selection.")
        }
    }

    FolderDialog {
        id: folderPicker
        title: qsTr("Choose a destination folder")
        onAccepted: root.controller.provideDestination(selectedFolder)
        onRejected: root.controller.cancelDestination()
    }
    FileDialog {
        id: filePicker
        title: qsTr("Choose files for Donut")
        fileMode: FileDialog.OpenFiles
        onAccepted: root.controller.setPayloadUrls(selectedFiles)
    }
    Connections {
        target: root.controller
        function onDestinationRequested(action, suggestedFolder) {
            folderPicker.currentFolder = suggestedFolder
            folderPicker.open()
        }
    }
}
