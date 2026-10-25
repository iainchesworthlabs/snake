// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// The end of a game: the board as it was left, the score against the best, and the way on.
FocusScope {
    id: root

    required property GameViewModel game
    required property ProfileViewModel profile

    // What the game came to, in words: a new best, how far short it fell, or nothing when there is no best to speak of.
    readonly property string verdict: {
        if (profile.newBest)
            return qsTr("New best!");
        if (profile.pointsToBest > 0)
            return qsTr("%L1 more to beat your best.").arg(profile.pointsToBest);
        if (profile.pointsToBest === 0)
            return qsTr("You matched your best.");
        return "";
    }

    // Each time the screen shows, the keyboard starts on its main button.
    onVisibleChanged: {
        if (visible)
            playAgainButton.forceActiveFocus();
    }

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Game over")

    PhonePage {
        ShadowText {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 28
            offset: 4
            pixelSize: 48
            text: qsTr("Game over")
        }

        BoardItem {
            x: 0
            y: 100
            width: 390
            height: 390
            game: root.game
        }

        Item {
            x: 15
            y: 502
            width: 360
            height: panel.height + 6

            Rectangle {
                y: 6
                width: parent.width
                height: panel.height
                radius: 18
                color: Theme.shadow
            }

            Rectangle {
                id: panel

                width: parent.width
                height: panelColumn.height + 36
                radius: 18
                color: Theme.surface
                border.width: 4
                border.color: Theme.ink

                Column {
                    id: panelColumn

                    x: 18
                    y: 18
                    width: parent.width - 36
                    spacing: 12

                    Item {
                        width: parent.width
                        height: scoreColumn.height

                        Column {
                            id: scoreColumn

                            spacing: 4

                            Text {
                                color: Theme.ink
                                font.pixelSize: 15
                                font.weight: Font.Medium
                                text: qsTr("Score")
                            }

                            Text {
                                objectName: "finalScore"
                                color: Theme.ink
                                font.pixelSize: 44
                                font.weight: Font.Bold
                                text: root.game.score
                            }
                        }

                        Column {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            spacing: 6

                            Text {
                                anchors.right: parent.right
                                color: Theme.ink
                                font.pixelSize: 15
                                font.weight: Font.Medium
                                text: qsTr("Best")
                            }

                            Rectangle {
                                width: bestText.width + 20
                                height: bestText.height + 12
                                radius: 8
                                color: Theme.ink

                                Text {
                                    id: bestText

                                    anchors.centerIn: parent
                                    color: Theme.surface
                                    font.pixelSize: 22
                                    font.weight: Font.Bold
                                    text: root.profile.best
                                }
                            }
                        }
                    }

                    Text {
                        objectName: "verdict"
                        visible: text !== ""
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Theme.ink
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        text: root.verdict
                    }

                    Text {
                        objectName: "unlocked"
                        visible: text !== ""
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Theme.ink
                        font.pixelSize: 17
                        font.weight: Font.Bold
                        text: root.profile.unlockedNow.map(name => qsTr("%1 unlocked!").arg(name)).join("\n")
                    }
                }
            }
        }

        ToyButton {
            id: playAgainButton

            objectName: "playAgainButton"
            x: 15
            y: 690
            width: 360
            height: 60
            focus: true
            kind: "primary"
            fontSize: 26
            drop: 6
            radius: 14
            text: qsTr("Play again")
            onClicked: root.game.restart()
        }

        ToyButton {
            objectName: "menuButton"
            x: 15
            y: 762
            width: 360
            height: 50
            text: qsTr("Menu")
            onClicked: root.game.newGame()
        }
    }
}
