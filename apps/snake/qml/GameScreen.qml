// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The game: the score and the best score above the board. The keys go to the game. This layout is a stand-in for the
// design's phone and desktop pages.
FocusScope {
    id: root

    required property GameViewModel game
    required property ProfileViewModel profile
    required property EffectsModel effects
    required property InputController input

    // On a phone there are controls to show: a pause button, and under the board the d-pad, the joystick or the tilt indicator.
    readonly property bool touchControls: input.controls !== InputController.Scheme.Keys

    // The parts of the screen that a phone covers: the status bar and the camera at the top, the home indicator at the bottom,
    // and the rounded corners. The score, the best and the controls stay inside them. Zero on a computer.
    readonly property real safeTop: SafeArea.margins.top
    readonly property real safeBottom: SafeArea.margins.bottom
    readonly property real safeLeft: SafeArea.margins.left
    readonly property real safeRight: SafeArea.margins.right

    // The score on the leading side and the best on the other: the other way round in a language read from right to left.
    LayoutMirroring.enabled: Theme.rightToLeft
    LayoutMirroring.childrenInherit: true

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Game")

    Keys.onPressed: event => {
        if (root.game.state === GameViewModel.State.GameOver) {
            // The snake is dying; a key is "get on with it".
            root.effects.skipDeath();
            event.accepted = true;
        } else {
            event.accepted = root.game.handleKey(event.key, event.isAutoRepeat);
        }
    }

    TapHandler {
        enabled: root.game.state === GameViewModel.State.GameOver
        onTapped: root.effects.skipDeath()
    }

    // A swipe anywhere on the screen turns the snake, when swiping is how it is steered. The positions are in
    // device-independent pixels, which is what the swipe's length is measured in.
    MultiPointTouchArea {
        objectName: "swipeArea"
        anchors.fill: parent
        enabled: root.input.controls === InputController.Scheme.Swipe && root.game.state === GameViewModel.State.Running
        mouseEnabled: false
        minimumTouchPoints: 1
        maximumTouchPoints: 5
        onPressed: points => {
            for (const point of points)
                root.input.touchPressed(point.pointId, point.x, point.y);
        }
        onUpdated: points => {
            for (const point of points)
                root.input.touchMoved(point.pointId, point.x, point.y);
        }
        onReleased: points => {
            for (const point of points)
                root.input.touchReleased(point.pointId);
        }
        onCanceled: root.input.touchCancelled()
    }

    // Pause, for a player with no keyboard: a square button with two bars, at the bottom on the leading side. The pause dialog
    // has Resume, Restart and Quit to menu, so this is also the way to stop a game. It is not at the top because the score
    // there can be six digits wide.
    ToyButton {
        objectName: "pauseButton"
        visible: root.touchControls
        anchors.left: parent.left
        anchors.leftMargin: 16 + root.safeLeft
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24 + root.safeBottom
        width: 56
        height: 56
        radius: 14
        activeFocusOnTab: false
        Accessible.name: qsTr("Pause")
        onClicked: root.game.pause()

        Row {
            anchors.centerIn: parent
            spacing: 6

            Repeater {
                model: 2

                Rectangle {
                    width: 6
                    height: 20
                    color: Theme.ink
                }
            }
        }
    }

    DPad {
        objectName: "dpad"
        visible: root.input.controls === InputController.Scheme.Dpad
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24 + root.safeBottom
        input: root.input
    }

    JoystickPad {
        objectName: "joystick"
        visible: root.input.controls === InputController.Scheme.Joystick
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24 + root.safeBottom
        input: root.input
    }

    TiltIndicator {
        objectName: "tiltIndicator"
        visible: root.input.controls === InputController.Scheme.Tilt
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 48 + root.safeBottom
        input: root.input
    }

    BoardItem {
        id: board

        objectName: "board"
        anchors.fill: parent
        anchors.leftMargin: 16 + root.safeLeft
        anchors.rightMargin: 16 + root.safeRight
        anchors.topMargin: 64 + root.safeTop
        // Room for the d-pad (208 high, 24 up), the joystick (184 high, 24 up) or the tilt indicator (132 high, 48 up) and a
        // gap above it; with swiping, for the pause button (56 high, 24 up).
        anchors.bottomMargin: root.safeBottom + (root.input.controls === InputController.Scheme.Dpad ? 242 : root.input.controls === InputController.Scheme.Joystick ? 218 : root.input.controls === InputController.Scheme.Tilt ? 196 : root.touchControls ? 96 : 16)
        game: root.game
        effects: root.effects

        // With reduced motion the food stays where it is.
        Binding {
            target: board
            property: "foodLift"
            value: 0
            when: root.profile.reducedMotion
        }

        // The food bobs up and down. Each half is an ease-in-out, as in the design.
        SequentialAnimation on foodLift {
            loops: Animation.Infinite
            running: root.game.state !== GameViewModel.State.Paused && !root.profile.reducedMotion

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
        anchors.leftMargin: 16 + root.safeLeft
        anchors.topMargin: 16 + root.safeTop

        Text {
            color: Theme.ink
            font.pixelSize: 16
            font.weight: Font.Medium
            text: qsTr("Score")
        }

        Text {
            objectName: "score"
            color: Theme.ink
            font.pixelSize: 46
            font.weight: Font.Bold
            text: root.game.score
        }
    }

    Column {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 16 + root.safeRight
        anchors.topMargin: 16 + root.safeTop
        spacing: 4

        Text {
            anchors.right: parent.right
            color: Theme.ink
            font.pixelSize: 16
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

                objectName: "bestScore"
                anchors.centerIn: parent
                color: Theme.surface
                font.pixelSize: 24
                font.weight: Font.Bold
                text: root.profile.best
            }
        }
    }
}
