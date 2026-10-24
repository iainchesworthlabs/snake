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
    color: "#101820"

    // A game that loses the focus waits for the player.
    onActiveChanged: {
        if (!active)
            game.pause();
    }

    GameViewModel {
        id: game
    }

    // The frame animation runs in step with the display. Each frame it tells the game how long the frame took, and the
    // game runs the ticks that are due.
    FrameAnimation {
        running: game.state === GameViewModel.State.Running
        onTriggered: game.advance(frameTime)
    }

    Item {
        anchors.fill: parent
        focus: true

        Keys.onPressed: event => {
            event.accepted = game.handleKey(event.key, event.isAutoRepeat);
        }

        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 16
            color: "#e0e6ea"
            font.pixelSize: 24
            text: qsTr("Score %1").arg(game.score)
        }

        Text {
            anchors.centerIn: parent
            color: "#e0e6ea"
            font.pixelSize: 24
            visible: text !== ""
            text: {
                switch (game.state) {
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
