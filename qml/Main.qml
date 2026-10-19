// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Dialogs
import org.kde.donut.core

// Overlay surface used by the daemon. OverlayWindow (C++) sizes, places and
// shows it as a layer-shell surface; QML only renders and forwards input.
Window {
    id: root

    required property DonutController controller
    required property SettingsManager settings

    title: "Donut"
    color: "transparent"
    flags: Qt.FramelessWindowHint
    visible: false
    width: 400
    height: 460

    DonutMenu {
        id: menu
        controller: root.controller
        settings: root.settings
        x: (root.width - width) / 2
        y: 0
        onOpenFinished: root.controller.notifyOpened()
        onCloseFinished: root.controller.notifyClosed()
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

    // Layer-shell surfaces cannot parent xdg dialogs, so dialogs are top-level.
    FolderDialog {
        id: folderPicker
        parentWindow: null
        title: qsTr("Choose a destination folder")
        onAccepted: root.controller.provideDestination(selectedFolder)
        onRejected: root.controller.cancelDestination()
    }
    FileDialog {
        id: filePicker
        parentWindow: null
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
