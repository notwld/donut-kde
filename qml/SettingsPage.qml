// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.donut.core

ApplicationWindow {
    id: root

    required property SettingsManager settings
    required property ShortcutManager shortcuts
    required property DragBackend backend

    title: qsTr("Donut Settings")
    width: 560
    height: 720
    minimumWidth: 480
    minimumHeight: 480

    readonly property string shortcutConflict: shortcuts.conflictFor(shortcutField.text)

    component SectionTitle: Label {
        Layout.topMargin: 14
        Layout.fillWidth: true
        font.bold: true
        font.pixelSize: 15
    }
    component Hint: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        opacity: 0.7
        font.pixelSize: 11
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth - 32
            x: 16
            spacing: 6

            // --- Status ----------------------------------------------------
            Frame {
                Layout.fillWidth: true
                Layout.topMargin: 12
                Label {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: root.backend ? root.backend.modeDescription : ""
                }
            }

            SectionTitle { text: qsTr("General") }
            CheckBox {
                text: qsTr("Enable Donut")
                checked: root.settings.enabled
                onToggled: root.settings.enabled = checked
            }
            CheckBox {
                text: qsTr("Start automatically at login")
                checked: root.settings.autostart
                onToggled: root.settings.autostart = checked
            }
            CheckBox {
                text: qsTr("Diagnostic logging (donut.* debug output)")
                checked: root.settings.diagnosticLogging
                onToggled: root.settings.diagnosticLogging = checked
            }

            SectionTitle { text: qsTr("Trigger") }
            GridLayout {
                columns: 2
                columnSpacing: 12
                Layout.fillWidth: true
                Label { text: qsTr("Open during a drag:") }
                ComboBox {
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { value: "dwell", text: qsTr("When I pause while dragging") },
                        { value: "distance", text: qsTr("On every drag past the threshold") },
                        { value: "shortcut", text: qsTr("Never (shortcut only)") }
                    ]
                    currentIndex: indexOfValue(root.settings.triggerMode)
                    onActivated: root.settings.triggerMode = currentValue
                }
                Label { text: qsTr("Activation threshold:") }
                SpinBox {
                    from: 4; to: 96
                    value: root.settings.activationThreshold
                    textFromValue: (v) => qsTr("%1 px").arg(v)
                    onValueModified: root.settings.activationThreshold = value
                }
                Label { text: qsTr("Pause duration:"); enabled: root.settings.triggerMode === "dwell" }
                SpinBox {
                    enabled: root.settings.triggerMode === "dwell"
                    from: 150; to: 2000; stepSize: 50
                    value: root.settings.dwellMs
                    textFromValue: (v) => qsTr("%1 ms").arg(v)
                    onValueModified: root.settings.dwellMs = value
                }
                Label { text: qsTr("Manual shortcut:") }
                RowLayout {
                    TextField {
                        id: shortcutField
                        Layout.fillWidth: true
                        text: root.settings.shortcut
                        placeholderText: "Meta+Shift+D"
                        onEditingFinished: if (root.shortcutConflict.length === 0) root.settings.shortcut = text
                    }
                }
            }
            Hint {
                visible: root.shortcutConflict.length > 0
                color: "#FF5D73"
                opacity: 1
                text: root.shortcutConflict
            }
            Hint { text: root.shortcuts.status }
            Hint {
                text: qsTr("“Pause” keeps ordinary drags untouched: the donut only appears when you hold the pointer still mid-drag. Drop anywhere else and the drag works as usual.")
            }

            SectionTitle { text: qsTr("Appearance") }
            GridLayout {
                columns: 2
                columnSpacing: 12
                Layout.fillWidth: true
                Label { text: qsTr("Radius:") }
                Slider {
                    Layout.fillWidth: true
                    from: 120; to: 240; stepSize: 5
                    value: root.settings.outerRadius
                    onMoved: root.settings.outerRadius = value
                }
                Label { text: qsTr("Hub radius:") }
                Slider {
                    Layout.fillWidth: true
                    from: 36; to: 70; stepSize: 1
                    value: root.settings.innerRadius
                    onMoved: root.settings.innerRadius = value
                }
                Label { text: qsTr("Sector spacing:") }
                Slider {
                    Layout.fillWidth: true
                    from: 0; to: 10; stepSize: 0.5
                    value: root.settings.sectorSpacing
                    onMoved: root.settings.sectorSpacing = value
                }
                Label { text: qsTr("Opening animation:") }
                SpinBox {
                    from: 0; to: 600; stepSize: 20
                    value: root.settings.openDurationMs
                    textFromValue: (v) => qsTr("%1 ms").arg(v)
                    onValueModified: root.settings.openDurationMs = value
                }
                Label { text: qsTr("Glow intensity:") }
                Slider {
                    Layout.fillWidth: true
                    from: 0; to: 1
                    value: root.settings.glowIntensity
                    onMoved: root.settings.glowIntensity = value
                }
                Label { text: qsTr("Placement:") }
                ComboBox {
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { value: "centered", text: qsTr("Centred on the pointer") },
                        { value: "offset", text: qsTr("Offset up-left of the pointer") }
                    ]
                    currentIndex: indexOfValue(root.settings.placement)
                    onActivated: root.settings.placement = currentValue
                }
                Label { text: qsTr("Offset:"); enabled: root.settings.placement === "offset" }
                SpinBox {
                    enabled: root.settings.placement === "offset"
                    from: 0; to: 200; stepSize: 4
                    value: root.settings.placementOffset
                    textFromValue: (v) => qsTr("%1 px").arg(v)
                    onValueModified: root.settings.placementOffset = value
                }
            }
            CheckBox {
                text: qsTr("Show labels")
                checked: root.settings.showLabels
                onToggled: root.settings.showLabels = checked
            }
            CheckBox {
                text: qsTr("Blur the desktop behind the donut")
                checked: root.settings.blurBehind
                onToggled: root.settings.blurBehind = checked
            }
            CheckBox {
                text: qsTr("Reduce motion (disable animations)")
                checked: root.settings.reducedMotion
                onToggled: root.settings.reducedMotion = checked
            }
            Hint { text: qsTr("Animations also follow Plasma's global animation speed.") }

            SectionTitle { text: qsTr("Actions") }
            Repeater {
                model: [
                    { id: "copy", text: qsTr("Copy") },
                    { id: "move", text: qsTr("Move") },
                    { id: "zip", text: qsTr("Zip") },
                    { id: "extract", text: qsTr("Extract") }
                ]
                delegate: CheckBox {
                    required property var modelData
                    text: modelData.text
                    checked: root.settings.enabledActions.indexOf(modelData.id) >= 0
                    onToggled: root.settings.setActionEnabled(modelData.id, checked)
                }
            }
            Hint { text: qsTr("Cancel is always available.") }
            GridLayout {
                columns: 2
                columnSpacing: 12
                Layout.fillWidth: true
                Label { text: qsTr("Copy / Move destination:") }
                ComboBox {
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { value: "ask", text: qsTr("Ask every time") },
                        { value: "fixed", text: qsTr("Always the same folder") }
                    ]
                    currentIndex: indexOfValue(root.settings.destinationMode)
                    onActivated: root.settings.destinationMode = currentValue
                }
                Label { text: qsTr("Folder:"); enabled: root.settings.destinationMode === "fixed" }
                RowLayout {
                    enabled: root.settings.destinationMode === "fixed"
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                        text: root.settings.fixedDestination.toString().length > 0 ? root.settings.fixedDestination.toString().replace("file://", "") : qsTr("(none)")
                    }
                    Button {
                        text: qsTr("Choose…")
                        onClicked: fixedFolderDialog.open()
                    }
                }
            }
            Hint { text: qsTr("Zip and Extract save next to the original item. Nothing is ever overwritten: clashing names get a number.") }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.bottomMargin: 16
                Button {
                    text: qsTr("Defaults")
                    onClicked: root.settings.resetToDefaults()
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Revert")
                    enabled: root.settings.dirty
                    onClicked: root.settings.load()
                }
                Button {
                    text: qsTr("Save")
                    highlighted: true
                    enabled: root.settings.dirty && root.shortcutConflict.length === 0
                    onClicked: {
                        root.settings.shortcut = shortcutField.text
                        root.settings.save()
                    }
                }
            }
        }
    }

    FolderDialog {
        id: fixedFolderDialog
        title: qsTr("Destination for Copy and Move")
        onAccepted: root.settings.fixedDestination = selectedFolder
    }
}
