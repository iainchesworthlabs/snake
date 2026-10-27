// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// The pause dialog over the dimmed game: resume, restart or leave for the menu.
FocusScope {
    id: root

    required property GameViewModel game
    required property ProfileViewModel profile
    // How the snake is steered now, and whether the device can feel its tilt. The way of steering is chosen only where there
    // is a touch screen to steer with.
    required property InputController input

    // Each time the screen shows, the keyboard starts on its main button.
    onVisibleChanged: {
        if (visible)
            resumeButton.forceActiveFocus();
    }

    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Paused")

    Keys.onEscapePressed: root.input.resume()

    // The dimming covers the whole window and takes the clicks, so nothing below it is touched.
    Rectangle {
        anchors.fill: parent
        color: Theme.overlay

        MouseArea {
            anchors.fill: parent
        }
    }

    PhonePage {
        Item {
            anchors.centerIn: parent
            width: 330
            height: dialogColumn.height + 44

            Rectangle {
                y: 8
                width: parent.width
                height: parent.height
                radius: 20
                color: Theme.shadow
            }

            Rectangle {
                width: parent.width
                height: parent.height
                radius: 20
                color: Theme.surface
                border.width: 4
                border.color: Theme.ink
            }

            Column {
                id: dialogColumn

                x: 22
                y: 22
                width: parent.width - 44
                spacing: 14

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.ink
                    font.pixelSize: 38
                    font.weight: Font.Bold
                    text: qsTr("Paused")
                }

                Column {
                    width: parent.width
                    spacing: 15

                    ToyButton {
                        id: resumeButton

                        objectName: "resumeButton"
                        width: parent.width
                        height: 56
                        focus: true
                        kind: "primary"
                        fontSize: 22
                        drop: 5
                        text: qsTr("Resume")
                        onClicked: root.input.resume()
                    }

                    ToyButton {
                        objectName: "restartButton"
                        width: parent.width
                        height: 48
                        kind: "accent"
                        text: qsTr("Restart")
                        onClicked: root.game.restart()
                    }
                }

                // How to steer: only where there is a touch screen to steer with.
                Column {
                    visible: root.input.controls !== InputController.Scheme.Keys
                    width: parent.width
                    spacing: 8

                    Text {
                        width: parent.width
                        color: Theme.ink
                        font.pixelSize: 16
                        font.weight: Font.Medium
                        text: qsTr("Controls")
                    }

                    Grid {
                        columns: 2
                        spacing: 8

                        Repeater {
                            model: [
                                {
                                    name: "Tilt",
                                    label: qsTr("Tilt"),
                                    value: ProfileViewModel.Controls.Tilt,
                                    scheme: InputController.Scheme.Tilt
                                },
                                {
                                    name: "Swipe",
                                    label: qsTr("Swipe"),
                                    value: ProfileViewModel.Controls.Swipe,
                                    scheme: InputController.Scheme.Swipe
                                },
                                {
                                    name: "Dpad",
                                    label: qsTr("D-pad"),
                                    value: ProfileViewModel.Controls.Dpad,
                                    scheme: InputController.Scheme.Dpad
                                },
                                {
                                    name: "Joystick",
                                    label: qsTr("Joystick"),
                                    value: ProfileViewModel.Controls.Joystick,
                                    scheme: InputController.Scheme.Joystick
                                }
                            ]

                            ToyButton {
                                id: choice

                                required property var modelData

                                objectName: "controls" + choice.modelData.name
                                // A device that cannot feel its tilt is not offered it.
                                visible: choice.modelData.scheme !== InputController.Scheme.Tilt || root.input.tiltAvailable
                                width: 139
                                height: 42
                                kind: root.input.controls === choice.modelData.scheme ? "chosen" : "secondary"
                                activeFocusOnTab: false
                                fontSize: 16
                                drop: 3
                                text: choice.modelData.label
                                onClicked: root.profile.controls = choice.modelData.value
                            }
                        }
                    }

                    ToyButton {
                        objectName: "recalibrateButton"
                        visible: root.input.controls === InputController.Scheme.Tilt
                        width: parent.width
                        height: 44
                        fontSize: 17
                        text: qsTr("Recalibrate tilt")
                        onClicked: root.input.startCalibration()
                    }

                    // How far the phone must be tipped to steer: gentle for a player who tips it too easily, quick for one who finds
                    // the usual hard to reach.
                    Text {
                        visible: root.input.controls === InputController.Scheme.Tilt
                        width: parent.width
                        color: Theme.ink
                        font.pixelSize: 16
                        font.weight: Font.Medium
                        text: qsTr("Tilt sensitivity")
                    }

                    Row {
                        visible: root.input.controls === InputController.Scheme.Tilt
                        spacing: 8

                        Repeater {
                            model: [
                                {
                                    name: "Gentle",
                                    label: qsTr("Gentle"),
                                    value: ProfileViewModel.TiltSensitivity.Gentle
                                },
                                {
                                    name: "Normal",
                                    label: qsTr("Normal"),
                                    value: ProfileViewModel.TiltSensitivity.Normal
                                },
                                {
                                    name: "Quick",
                                    label: qsTr("Quick"),
                                    value: ProfileViewModel.TiltSensitivity.Quick
                                }
                            ]

                            ToyButton {
                                id: sensitivity

                                required property var modelData

                                objectName: "sensitivity" + sensitivity.modelData.name
                                width: 90
                                height: 42
                                kind: root.profile.tiltSensitivity === sensitivity.modelData.value ? "chosen" : "secondary"
                                activeFocusOnTab: false
                                fontSize: 16
                                drop: 3
                                text: sensitivity.modelData.label
                                onClicked: root.profile.tiltSensitivity = sensitivity.modelData.value
                            }
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 10

                    ToggleSwitch {
                        objectName: "pauseSoundToggle"
                        width: parent.width
                        text: qsTr("Sound")
                        checked: root.profile.sound
                        onClicked: root.profile.sound = !root.profile.sound
                    }

                    ToggleSwitch {
                        objectName: "pauseMusicToggle"
                        width: parent.width
                        text: qsTr("Music")
                        checked: root.profile.music
                        onClicked: root.profile.music = !root.profile.music
                    }
                }

                ToyButton {
                    objectName: "quitButton"
                    width: parent.width
                    height: 48
                    kind: "secondary"
                    fontSize: 18
                    text: qsTr("Quit to menu")
                    onClicked: root.game.newGame()
                }
            }
        }
    }
}
