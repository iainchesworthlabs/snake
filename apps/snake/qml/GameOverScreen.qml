// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// The end of a game: the board as it was left, the score against the best, and the way on.
FocusScope {
    id: root

    required property GameViewModel game
    required property ProfileViewModel profile
    // The snake as the crash left it, a head with dizzy eyes.
    required property EffectsModel effects

    // The score made the top 10 and the player chose to give a name for it.
    signal nameRequested

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

    // The main button is the one that saves a score that made the top 10, and Play again otherwise. The keyboard
    // starts on it each time the screen shows and when the score turns out to qualify.
    function focusMainButton() {
        if (profile.qualifies)
            saveScoreButton.forceActiveFocus();
        else
            playAgainButton.forceActiveFocus();
    }

    onVisibleChanged: {
        if (visible)
            focusMainButton();
    }

    Connections {
        target: root.profile

        function onPendingChanged() {
            if (root.visible)
                root.focusMainButton();
        }
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
            effects: root.effects
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

                            anchors.left: parent.left
                            spacing: 4

                            Text {
                                color: Theme.ink
                                font.pixelSize: 15
                                font.weight: Font.Medium
                                text: qsTr("Score")
                            }

                            PixelNumber {
                                objectName: "finalScore"
                                cell: 6
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

                                PixelNumber {
                                    id: bestText

                                    anchors.centerIn: parent
                                    color: Theme.surface
                                    cell: 3
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
            id: saveScoreButton

            objectName: "saveScoreButton"
            visible: root.profile.qualifies
            x: 15
            y: 690
            width: 360
            height: 60
            kind: "primary"
            fontSize: 26
            drop: 6
            radius: 14
            text: qsTr("Save score")
            onClicked: root.nameRequested()
        }

        ToyButton {
            id: playAgainButton

            objectName: "playAgainButton"
            visible: !root.profile.qualifies
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
            onClicked: {
                root.profile.discardScore();
                root.game.newGame();
            }
        }
    }
}
