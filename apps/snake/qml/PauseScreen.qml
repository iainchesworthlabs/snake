// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// The pause dialog over the dimmed game: resume, restart or leave for the menu.
FocusScope {
    id: root

    required property GameViewModel game
    required property ProfileViewModel profile
    // True on a device with a touch screen, where the way of steering can be chosen.
    required property bool touchDevice

    // Each time the screen shows, the keyboard starts on its main button.
    onVisibleChanged: {
        if (visible)
            resumeButton.forceActiveFocus();
    }

    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Paused")

    Keys.onEscapePressed: root.game.resume()

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
                        onClicked: root.game.resume()
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
                    visible: root.touchDevice
                    width: parent.width
                    spacing: 8

                    Text {
                        width: parent.width
                        color: Theme.ink
                        font.pixelSize: 16
                        font.weight: Font.Medium
                        text: qsTr("Controls")
                    }

                    Row {
                        spacing: 8

                        Repeater {
                            model: [
                                {
                                    name: "Tilt",
                                    label: qsTr("Tilt"),
                                    value: ProfileViewModel.Controls.Tilt
                                },
                                {
                                    name: "Swipe",
                                    label: qsTr("Swipe"),
                                    value: ProfileViewModel.Controls.Swipe
                                },
                                {
                                    name: "Dpad",
                                    label: qsTr("D-pad"),
                                    value: ProfileViewModel.Controls.Dpad
                                }
                            ]

                            ToyButton {
                                id: choice

                                required property var modelData

                                objectName: "controls" + choice.modelData.name
                                width: 88
                                height: 42
                                kind: root.profile.controls === choice.modelData.value ? "chosen" : "secondary"
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
                        visible: root.profile.controls === ProfileViewModel.Controls.Tilt
                        width: parent.width
                        height: 44
                        fontSize: 17
                        text: qsTr("Recalibrate tilt")
                        onClicked: root.profile.calibrateTilt()
                    }

                    Text {
                        visible: root.profile.controls === ProfileViewModel.Controls.Tilt
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: Theme.ink
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        text: qsTr("Hold it how you like to play, then tap.")
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
                    height: 44
                    kind: "link"
                    fontSize: 17
                    text: qsTr("Quit to menu")
                    onClicked: root.game.newGame()
                }
            }
        }
    }
}
