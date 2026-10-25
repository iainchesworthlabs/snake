// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// What the player can change: sound, music, reduced motion and, on a touch device, inverted axes; and the reset of a
// leaderboard, which asks first. The keyboard moves between the rows with the up and down arrows.
FocusScope {
    id: root

    required property ProfileViewModel profile
    // True on a device with a touch screen, where tilt and the touch pad steer.
    required property bool touchDevice

    // The player left the screen: the title shows next.
    signal closed
    signal aboutRequested

    // The mode whose leaderboard the dialog asks about.
    property int resetMode: GameViewModel.Mode.Easy
    readonly property string resetModeName: resetMode === GameViewModel.Mode.Easy ? qsTr("Easy") : qsTr("Classic")

    onVisibleChanged: {
        if (visible) {
            confirm.visible = false;
            soundToggle.forceActiveFocus();
        }
    }

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Settings")

    Keys.onEscapePressed: root.closed()

    PhonePage {
        // The page without the dialog: it takes no focus or touch while the dialog is up.
        Item {
            anchors.fill: parent
            enabled: !confirm.visible

            ToyButton {
                objectName: "settingsBackButton"
                x: 15
                y: 28
                width: 46
                height: 46
                radius: 10
                text: "<"
                fontSize: 24
                Accessible.name: qsTr("Back")
                onClicked: root.closed()
            }

            ShadowText {
                x: 74
                y: 26
                offset: 3
                pixelSize: 34
                text: qsTr("Settings")
            }

            // The switches, in a panel of the design's kind.
            Item {
                x: 15
                y: 100
                width: 360
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
                        spacing: 10

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
                x: 15
                y: 100 + switches.height + 26
                objectName: "resetLabel"
                color: Theme.ink
                font.pixelSize: 17
                font.weight: Font.DemiBold
                text: qsTr("Reset a leaderboard")
            }

            Row {
                x: 15
                y: 100 + switches.height + 56
                spacing: 8

                ToyButton {
                    objectName: "resetEasyButton"
                    width: 176
                    height: 46
                    text: qsTr("Easy")
                    onClicked: {
                        root.resetMode = GameViewModel.Mode.Easy;
                        confirm.visible = true;
                    }
                }

                ToyButton {
                    objectName: "resetClassicButton"
                    width: 176
                    height: 46
                    text: qsTr("Classic")
                    onClicked: {
                        root.resetMode = GameViewModel.Mode.Classic;
                        confirm.visible = true;
                    }
                }
            }

            ToyButton {
                objectName: "aboutButton"
                x: 15
                y: 100 + switches.height + 56 + 46 + 40
                width: 360
                height: 50
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
            confirm.visible = false;
        }
        onCancelled: confirm.visible = false
    }
}
