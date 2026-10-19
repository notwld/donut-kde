// SPDX-License-Identifier: GPL-2.0-or-later
// Full-screen drag source for the nested-KWin end-to-end test.
// DONUT_E2E_URI is the text/uri-list payload to drag.
import QtQuick

Window {
    visible: true
    visibility: Window.FullScreen
    color: "steelblue"
    Item {
        anchors.fill: parent
        Drag.active: dh.active
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
        Drag.mimeData: { "text/uri-list": Qt.application.arguments[Qt.application.arguments.length - 1] }
        Drag.onDragStarted: console.warn("E2E-SOURCE: drag started")
        Drag.onDragFinished: dropAction => console.warn("E2E-SOURCE: drag finished, action " + dropAction)
        DragHandler { id: dh }
    }
}
