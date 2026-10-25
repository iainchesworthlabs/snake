// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Window {
    id: root

    required property int windowWidth
    required property int windowHeight

    width: windowWidth
    height: windowHeight
    visible: true
    title: qsTr("Snake")
    color: Theme.background

    // A game that loses the focus waits for the player.
    onActiveChanged: {
        if (!active)
            gameModel.pause();
    }

    GameViewModel {
        id: gameModel
    }

    // The frame animation runs in step with the display. Each frame it tells the game how long the frame took, and the
    // game runs the ticks that are due.
    FrameAnimation {
        running: gameModel.state === GameViewModel.State.Running
        onTriggered: gameModel.advance(frameTime)
    }

    Item {
        anchors.fill: parent
        focus: true

        Keys.onPressed: event => {
            event.accepted = gameModel.handleKey(event.key, event.isAutoRepeat);
        }

        BoardItem {
            anchors.fill: parent
            anchors.margins: 16
            anchors.topMargin: 64
            game: gameModel

            // The food bobs up and down. Each half is an ease-in-out, as in the design.
            SequentialAnimation on foodLift {
                loops: Animation.Infinite
                running: gameModel.state !== GameViewModel.State.Paused

                NumberAnimation {
                    to: 1
                    duration: 450
                    easing.type: Easing.InOutQuad
                }

                NumberAnimation {
                    to: 0
                    duration: 450
                    easing.type: Easing.InOutQuad
                }
            }
        }

        Column {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 16

            Text {
                color: Theme.ink
                font.pixelSize: 16
                font.weight: Font.Medium
                text: qsTr("Score")
            }

            Text {
                color: Theme.ink
                font.pixelSize: 46
                font.weight: Font.Bold
                text: gameModel.score
            }
        }

        // What the player does next, in a panel. The screens that replace it come with the menus.
        Rectangle {
            anchors.centerIn: parent
            width: message.width + 48
            height: message.height + 32
            visible: message.text !== ""
            color: Theme.surface
            radius: 14
            border.width: 3
            border.color: Theme.ink

            Text {
                id: message

                anchors.centerIn: parent
                color: Theme.ink
                font.pixelSize: 26
                font.weight: Font.Bold
                text: {
                    switch (gameModel.state) {
                    case GameViewModel.State.NotStarted:
                        return qsTr("Press Enter to start");
                    case GameViewModel.State.Paused:
                        return qsTr("Paused. Press Space to resume.");
                    case GameViewModel.State.GameOver:
                        return qsTr("Game over. Press Enter to play again.");
                    default:
                        return "";
                    }
                }
            }
        }
    }
}
