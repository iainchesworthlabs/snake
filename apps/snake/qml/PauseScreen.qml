// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The pause dialog over the dimmed game: resume, restart or leave for the menu.
FocusScope {
    id: root

    required property GameViewModel game

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
