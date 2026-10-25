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

    // The title starts on Easy, as the design does. The mode played last will come back with the settings.
    Component.onCompleted: gameModel.mode = GameViewModel.Mode.Easy

    // A game that loses the focus waits for the player.
    onActiveChanged: {
        if (!active)
            gameModel.pause();
    }

    GameViewModel {
        id: gameModel

        objectName: "game"
    }

    ProfileViewModel {
        id: profileModel

        objectName: "profile"
        game: gameModel
    }

    // The frame animation runs in step with the display. Each frame it tells the game how long the frame took, and the
    // game runs the ticks that are due.
    FrameAnimation {
        running: gameModel.state === GameViewModel.State.Running
        onTriggered: gameModel.advance(frameTime)
    }

    // The screen follows the state of the game: the title before a game, the board while it runs, the pause dialog over
    // the board, and the end screen after it. The screen that shows takes the keyboard.
    TitleScreen {
        objectName: "titleScreen"
        anchors.fill: parent
        visible: gameModel.state === GameViewModel.State.NotStarted
        focus: visible
        game: gameModel
        profile: profileModel
    }

    GameScreen {
        objectName: "gameScreen"
        anchors.fill: parent
        visible: gameModel.state === GameViewModel.State.Running || gameModel.state === GameViewModel.State.Paused
        focus: gameModel.state === GameViewModel.State.Running
        game: gameModel
        profile: profileModel
    }

    PauseScreen {
        objectName: "pauseScreen"
        anchors.fill: parent
        visible: gameModel.state === GameViewModel.State.Paused
        focus: visible
        game: gameModel
    }

    GameOverScreen {
        objectName: "gameOverScreen"
        anchors.fill: parent
        visible: gameModel.state === GameViewModel.State.GameOver
        focus: visible
        game: gameModel
        profile: profileModel
    }
}
