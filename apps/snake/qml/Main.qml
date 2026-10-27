// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Window {
    id: root

    required property int windowWidth
    required property int windowHeight
    // Where the profile is kept. Empty keeps it in memory only, as the tests do.
    property string saveFile
    // True on a device with a touch screen.
    property bool touchDevice

    width: windowWidth
    height: windowHeight
    // On a phone the window fills the screen, under the status bar, the camera cut-out and the home indicator, and the screens keep
    // inside the safe area (Window.safeAreaMargins). Without this the phone leaves bars of black above and below the game.
    flags: touchDevice ? (Qt.Window | Qt.ExpandedClientAreaHint) : Qt.Window
    visible: true
    title: qsTr("Snake")
    color: Theme.background

    // The screen shown over the title or the game over screen: "", "nameEntry", "leaderboard", "settings", "about" or "notice". A game that starts
    // clears it.
    property string overlay

    // The licence text the notice screen shows: "licence", "notices" or "font".
    property string noticeKind: "licence"
    readonly property string noticeTitle: noticeKind === "licence" ? qsTr("Licence") : noticeKind === "notices" ? qsTr("Third-party notices") : qsTr("Font licence")
    readonly property string noticeBody: noticeKind === "licence" ? AboutInfo.licenceText : noticeKind === "notices" ? AboutInfo.noticesText : AboutInfo.fontLicenceText

    function showLeaderboard() {
        overlay = "leaderboard";
    }

    // How the snake is steered on a phone or a tablet: tilt, a swipe or the d-pad. The keyboard goes straight to the game.
    InputController {
        id: inputModel

        objectName: "input"
        game: gameModel
        profile: profileModel
        touchDevice: root.touchDevice
    }

    // The title starts on Easy, as the design does. The mode played last will come back with the settings.
    Component.onCompleted: gameModel.mode = GameViewModel.Mode.Easy

    // A game that loses the focus waits for the player.
    onActiveChanged: {
        if (!active)
            gameModel.pause();
    }

    // A phone that turns during a game anyway (the orientation could not be held) waits for the player too: the layout is
    // new and the tilt was calibrated for the old one, and resuming calibrates it again.
    readonly property bool lyingDown: width > height
    onLyingDownChanged: {
        if (touchDevice && gameModel.state === GameViewModel.State.Running)
            gameModel.pause();
    }

    GameViewModel {
        id: gameModel

        objectName: "game"

        onStateChanged: {
            if (state === GameViewModel.State.Running)
                root.overlay = "";
        }
    }

    // What is heard: the effects of the game, the buttons, and the music of the title and the game.
    SoundDirector {
        game: gameModel
        profile: profileModel
        // Put away (a phone's home button, another program in front): no music, no sound. Inactive, as a call or the
        // notification shade makes it, is not put away: the game pauses itself and its title music goes on.
        active: Application.state === Qt.ApplicationActive || Application.state === Qt.ApplicationInactive
    }

    // A phone's screen stays as it is from Play to the title, so that tilting to steer cannot turn it; the menus turn with the phone.
    OrientationDirector {
        game: gameModel
        active: root.touchDevice
    }

    // What the hand feels, on a phone: a tap for eating and a knock for a crash.
    HapticsDirector {
        game: gameModel
    }

    ProfileViewModel {
        id: profileModel

        objectName: "profile"
        game: gameModel
        saveFile: root.saveFile
    }

    // The burst when the snake eats, the points that rise, and the shake of a crash.
    EffectsModel {
        id: effectsModel

        objectName: "effects"
        game: gameModel
        reducedMotion: profileModel.reducedMotion
    }

    FrameAnimation {
        running: effectsModel.active
        onTriggered: effectsModel.advance(frameTime)
    }

    // The frame animation runs in step with the display. Each frame it tells the game how long the frame took, and the
    // game runs the ticks that are due.
    FrameAnimation {
        running: gameModel.state === GameViewModel.State.Running || inputModel.calibrating
        onTriggered: {
            // The tilt is read first, so that a turn is in place for the tick that the frame runs. While it is calibrated the
            // game is held, and the frames only move the wait on.
            inputModel.update(frameTime);
            gameModel.advance(frameTime);
        }
    }

    // The screen follows the state of the game: the title before a game, the board while it runs, the pause dialog over
    // the board, and the end screen after it. The screen that shows takes the keyboard.
    TitleScreen {
        objectName: "titleScreen"
        anchors.fill: parent
        visible: gameModel.state === GameViewModel.State.NotStarted && root.overlay === ""
        focus: visible
        game: gameModel
        profile: profileModel
        onLeaderboardRequested: {
            profileModel.boardMode = gameModel.mode;
            root.overlay = "leaderboard";
        }
        onSettingsRequested: root.overlay = "settings"
    }

    GameScreen {
        objectName: "gameScreen"
        anchors.fill: parent
        visible: gameModel.state === GameViewModel.State.Running || gameModel.state === GameViewModel.State.Paused || (gameModel.state === GameViewModel.State.GameOver && effectsModel.dying)
        focus: gameModel.state === GameViewModel.State.Running || (gameModel.state === GameViewModel.State.GameOver && effectsModel.dying)
        game: gameModel
        profile: profileModel
        effects: effectsModel
        input: inputModel
    }

    PauseScreen {
        objectName: "pauseScreen"
        anchors.fill: parent
        // While tilt is being calibrated the game is held, and the game screen says so; the dialog waits.
        visible: gameModel.state === GameViewModel.State.Paused && !inputModel.calibrating
        focus: visible
        game: gameModel
        profile: profileModel
        input: inputModel
    }

    GameOverScreen {
        objectName: "gameOverScreen"
        anchors.fill: parent
        visible: gameModel.state === GameViewModel.State.GameOver && root.overlay === "" && !effectsModel.dying
        focus: visible
        game: gameModel
        profile: profileModel
        effects: effectsModel
        onNameRequested: root.overlay = "nameEntry"
    }

    NameEntryScreen {
        objectName: "nameEntryScreen"
        anchors.fill: parent
        visible: gameModel.state === GameViewModel.State.GameOver && root.overlay === "nameEntry"
        focus: visible
        profile: profileModel
        onSaved: root.overlay = "leaderboard"
    }

    SettingsScreen {
        objectName: "settingsScreen"
        anchors.fill: parent
        visible: root.overlay === "settings"
        focus: visible
        profile: profileModel
        touchDevice: root.touchDevice
        onClosed: root.overlay = ""
        onAboutRequested: root.overlay = "about"
    }

    AboutScreen {
        objectName: "aboutScreen"
        anchors.fill: parent
        profile: profileModel
        visible: root.overlay === "about"
        focus: visible
        onClosed: root.overlay = "settings"
        onNoticeRequested: kind => {
            root.noticeKind = kind;
            root.overlay = "notice";
        }
    }

    NoticeScreen {
        objectName: "noticeScreen"
        anchors.fill: parent
        visible: root.overlay === "notice"
        focus: visible
        title: root.noticeTitle
        body: root.noticeBody
        onClosed: root.overlay = "about"
    }

    LeaderboardScreen {
        objectName: "leaderboardScreen"
        anchors.fill: parent
        visible: root.overlay === "leaderboard"
        focus: visible
        game: gameModel
        profile: profileModel
        onClosed: {
            root.overlay = "";
            // Leaving after a game goes to the title, not back to the end screen of a score that is saved.
            if (gameModel.state === GameViewModel.State.GameOver)
                gameModel.newGame();
        }
    }
}
