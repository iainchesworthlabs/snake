// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// What the player can change: sound, music, the volume, reduced motion and, on a touch device, inverted axes; and the reset of a
// leaderboard, which asks first. The keyboard moves between the rows with the up and down arrows.
FocusScope {
    id: root

    required property ProfileViewModel profile
    // True on a device with a touch screen, where tilt and the touch pad steer.
    required property bool touchDevice

    // The player left the screen: the title shows next.
    signal closed
    signal aboutRequested

    // The mode whose leaderboard the dialog asks about, and the button that asked, which gets the keyboard back.
    property int resetMode: GameViewModel.Mode.Easy
    property Item resetOpener
    readonly property string resetModeName: resetMode === GameViewModel.Mode.Easy ? qsTr("Easy") : resetMode === GameViewModel.Mode.Classic ? qsTr("Classic") : resetMode === GameViewModel.Mode.Portals ? qsTr("Portals") : qsTr("Hot potato")

    onVisibleChanged: {
        if (visible) {
            confirm.visible = false;
            soundToggle.forceActiveFocus();
        }
    }

    // Takes the question away and gives the keyboard back to the button that asked it: the buttons behind the question
    // were switched off, and a switched off button loses the keyboard.
    function closeConfirm() {
        confirm.visible = false;
        if (resetOpener)
            resetOpener.forceActiveFocus();
    }

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Settings")

    Keys.onEscapePressed: root.closed()

    PhonePage {
        id: page

        landscapeLayout: true

        // Lying down the switches are on the leading side and the rest on the other; the width of each.
        readonly property real columnWidth: (pageWidth - 3 * 15) / 2
        readonly property real leadingX: Theme.mirrored(15, columnWidth, pageWidth)
        readonly property real trailingX: Theme.mirrored(2 * 15 + columnWidth, columnWidth, pageWidth)

        // The page without the dialog: it takes no focus or touch while the dialog is up.
        Item {
            anchors.fill: parent
            enabled: !confirm.visible

            ToyButton {
                objectName: "settingsBackButton"
                x: Theme.mirrored(15, width, page.pageWidth)
                y: page.landscape ? 10 : 28
                width: page.landscape ? 42 : 46
                height: page.landscape ? 42 : 46
                radius: 10
                text: LayoutMirroring.enabled ? ">" : "<"
                fontSize: 24
                Accessible.name: qsTr("Back")
                onClicked: root.closed()
            }

            ShadowText {
                x: Theme.mirrored(page.landscape ? 66 : 74, width, page.pageWidth)
                maxWidth: 300
                y: page.landscape ? 8 : 26
                offset: 3
                pixelSize: page.landscape ? 30 : 34
                text: qsTr("Settings")
            }

            // The switches, in a panel of the design's kind.
            Item {
                x: page.landscape ? page.leadingX : 15
                y: page.landscape ? 64 : 100
                width: page.landscape ? page.columnWidth : 360
                height: switches.height + 6

                Rectangle {
                    y: 6
                    width: parent.width
                    height: switches.height
                    radius: 18
                    color: Theme.shadow
                }

                Rectangle {
                    id: switches

                    width: parent.width
                    height: switchColumn.height + 32
                    radius: 18
                    color: Theme.surface
                    border.width: 4
                    border.color: Theme.ink

                    Column {
                        id: switchColumn

                        x: 22
                        y: 16
                        width: parent.width - 44
                        spacing: page.landscape ? 6 : 10

                        ToggleSwitch {
                            id: soundToggle

                            objectName: "soundToggle"
                            width: parent.width
                            text: qsTr("Sound")
                            checked: root.profile.sound
                            focus: true
                            onClicked: root.profile.sound = !root.profile.sound
                        }

                        ToggleSwitch {
                            objectName: "musicToggle"
                            width: parent.width
                            text: qsTr("Music")
                            checked: root.profile.music
                            onClicked: root.profile.music = !root.profile.music
                        }

                        VolumeControl {
                            objectName: "volumeControl"
                            width: parent.width
                            text: qsTr("Volume")
                            value: root.profile.volume
                            maximum: root.profile.volumeMax
                            onStepped: level => root.profile.volume = level
                        }

                        ToggleSwitch {
                            objectName: "reducedMotionToggle"
                            width: parent.width
                            text: qsTr("Reduced motion")
                            checked: root.profile.reducedMotion
                            onClicked: root.profile.reducedMotion = !root.profile.reducedMotion
                        }

                        ToggleSwitch {
                            objectName: "invertAxesToggle"
                            visible: root.touchDevice
                            width: parent.width
                            text: qsTr("Invert axes")
                            checked: root.profile.invertAxes
                            onClicked: root.profile.invertAxes = !root.profile.invertAxes
                        }
                    }
                }
            }

            Text {
                x: page.landscape ? Theme.mirrored(2 * 15 + page.columnWidth, width, page.pageWidth) : Theme.mirrored(15, width, 390)
                y: page.landscape ? 70 : 100 + switches.height + 26
                objectName: "resetLabel"
                color: Theme.ink
                font.pixelSize: 17
                font.weight: Font.DemiBold
                text: qsTr("Reset a leaderboard")
            }

            // One button for each mode: two rows of two.
            Grid {
                id: resets

                x: page.landscape ? page.trailingX : 15
                y: page.landscape ? 98 : 100 + switches.height + 56
                columns: 2
                spacing: 8

                Repeater {
                    model: [
                        {
                            "id": "Easy",
                            "mode": GameViewModel.Mode.Easy,
                            "name": qsTr("Easy")
                        },
                        {
                            "id": "Classic",
                            "mode": GameViewModel.Mode.Classic,
                            "name": qsTr("Classic")
                        },
                        {
                            "id": "Portals",
                            "mode": GameViewModel.Mode.Portals,
                            "name": qsTr("Portals")
                        },
                        {
                            "id": "HotPotato",
                            "mode": GameViewModel.Mode.HotPotato,
                            "name": qsTr("Hot potato")
                        }
                    ]

                    ToyButton {
                        id: resetButton

                        required property var modelData

                        objectName: "reset" + resetButton.modelData.id + "Button"
                        width: page.landscape ? (page.columnWidth - 8) / 2 : 176
                        height: page.landscape ? 42 : 46
                        text: resetButton.modelData.name
                        onClicked: {
                            root.resetMode = resetButton.modelData.mode;
                            root.resetOpener = this;
                            confirm.visible = true;
                        }
                    }
                }
            }

            ToyButton {
                objectName: "aboutButton"
                x: page.landscape ? page.trailingX : 15
                y: page.landscape ? 98 + 42 + 8 + 42 + 14 : 100 + switches.height + 56 + 46 + 8 + 46 + 24
                width: page.landscape ? page.columnWidth : 360
                height: page.landscape ? 46 : 50
                text: qsTr("About")
                onClicked: root.aboutRequested()
            }
        }
    }

    ConfirmDialog {
        id: confirm

        objectName: "resetConfirm"
        anchors.fill: parent
        visible: false
        title: qsTr("Reset the %1 leaderboard?").arg(root.resetModeName)
        message: qsTr("Every score on it will be deleted. This cannot be undone.")
        confirmText: qsTr("Reset")
        cancelText: qsTr("Keep my scores")
        onConfirmed: {
            root.profile.resetLeaderboard(root.resetMode);
            root.closeConfirm();
        }
        onCancelled: root.closeConfirm()
    }
}
