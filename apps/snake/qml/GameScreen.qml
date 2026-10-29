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

    // Lying down (the window is wider than it is tall) the board takes the height and the rest is two columns beside it: the
    // score, the best and the pause button on the leading side and the controls on the other, as the design's wide pages have it.
    readonly property bool landscape: width > height
    readonly property real boardSide: height - safeTop - safeBottom - 24
    // The width of each column: what is left of the width after the board, but room enough for the score.
    readonly property real columnWidth: Math.max(190, (width - safeLeft - safeRight - boardSide) / 2)
    // How the steering controls are shrunk to fit the column, when it is narrower than they are.
    readonly property real controlsCentre: Theme.rightToLeft ? safeLeft + columnWidth / 2 : width - safeRight - columnWidth / 2
    function fitted(item) {
        return Math.min(1, (columnWidth - 16) / item.implicitWidth);
    }

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
        anchors.bottomMargin: (root.landscape ? 12 : 24) + root.safeBottom
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

    // Hot potato: drops the tail from the bomb. The bar over it is the fuse, which shortens as the bomb goes down the body to
    // the tail. Lying down it is beside the pause button; upright it is at the other end of the bottom row, which the d-pad,
    // the joystick and the tilt indicator leave free. On a computer the Enter key does the same.
    Item {
        id: shedControl

        objectName: "shedControl"
        visible: root.game.mode === GameViewModel.Mode.HotPotato
        // Placed by x and y, mirrored for a language read from right to left.
        x: Theme.mirrored(root.landscape ? 16 + root.safeLeft + (root.touchControls ? 56 + 12 : 0) : root.width - 16 - root.safeRight - width, width, root.width)
        y: root.height - height - (root.landscape ? 12 : 24) - root.safeBottom
        width: 72
        height: 56 + 14

        Rectangle {
            objectName: "fuseTrack"
            width: parent.width
            height: 8
            radius: 3
            color: "transparent"
            border.width: 2
            border.color: Theme.ink
            opacity: root.game.hasBomb ? 1 : 0.4

            Rectangle {
                objectName: "fuseBar"
                x: 2
                y: 2
                height: parent.height - 4
                width: (parent.width - 4) * root.game.fuse
                radius: 1
                color: root.game.fuse < 0.35 ? Theme.accent : Theme.ink
            }
        }

        ToyButton {
            objectName: "shedButton"
            y: 14
            width: parent.width
            height: 56
            radius: 14
            kind: root.game.canShed ? "primary" : "secondary"
            enabled: root.game.canShed
            activeFocusOnTab: false
            text: qsTr("Shed")
            Accessible.name: qsTr("Shed the tail from the bomb")
            onClicked: root.game.shed()
        }
    }

    // The wait while tilt is calibrated: the game is held, the phone is held how the player likes to play, and the bar fills. A tap
    // is felt when it is done.
    Rectangle {
        objectName: "calibrationOverlay"
        anchors.fill: parent
        visible: root.input.calibrating
        color: Theme.overlay

        MouseArea {
            anchors.fill: parent
        }

        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width - 64, 300)
            spacing: 18

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.surface
                font.pixelSize: 30
                font.weight: Font.Bold
                text: qsTr("Calibrating tilt")
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.surface
                font.pixelSize: 18
                font.weight: Font.Medium
                text: qsTr("Hold the phone how you like to play.")
            }

            Rectangle {
                width: parent.width
                height: 20
                radius: 6
                color: "transparent"
                border.width: 3
                border.color: Theme.surface

                Rectangle {
                    objectName: "calibrationBar"
                    x: 3
                    y: 3
                    height: parent.height - 6
                    width: (parent.width - 6) * root.input.calibrationProgress
                    radius: 3
                    color: Theme.accent
                }
            }
        }
    }

    DPad {
        id: dpad

        objectName: "dpad"
        visible: root.input.controls === InputController.Scheme.Dpad
        x: root.landscape ? root.controlsCentre - width / 2 : (root.width - width) / 2
        y: root.landscape ? (root.height - height) / 2 : root.height - height - 24 - root.safeBottom
        scale: root.landscape ? root.fitted(dpad) : 1
        input: root.input
    }

    JoystickPad {
        id: joystick

        objectName: "joystick"
        visible: root.input.controls === InputController.Scheme.Joystick
        x: root.landscape ? root.controlsCentre - width / 2 : (root.width - width) / 2
        y: root.landscape ? (root.height - height) / 2 : root.height - height - 24 - root.safeBottom
        scale: root.landscape ? root.fitted(joystick) : 1
        input: root.input
    }

    TiltIndicator {
        objectName: "tiltIndicator"
        visible: root.input.controls === InputController.Scheme.Tilt
        x: root.landscape ? root.controlsCentre - width / 2 : (root.width - width) / 2
        y: root.landscape ? (root.height - height) / 2 : root.height - height - 48 - root.safeBottom
        input: root.input
    }

    BoardItem {
        id: board

        objectName: "board"
        anchors.fill: parent
        anchors.leftMargin: root.landscape ? root.safeLeft + root.columnWidth : 16 + root.safeLeft
        anchors.rightMargin: root.landscape ? root.safeRight + root.columnWidth : 16 + root.safeRight
        anchors.topMargin: root.landscape ? 12 + root.safeTop : 64 + root.safeTop
        // Room for the d-pad (208 high, 24 up), the joystick (184 high, 24 up) or the tilt indicator (132 high, 48 up) and a
        // gap above it; with swiping, for the pause button (56 high, 24 up).
        anchors.bottomMargin: root.safeBottom + (root.landscape ? 12 : root.input.controls === InputController.Scheme.Dpad ? 242 : root.input.controls === InputController.Scheme.Joystick ? 218 : root.input.controls === InputController.Scheme.Tilt ? 196 : root.touchControls ? 96 : 16)
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
        id: scoreColumn

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 16 + root.safeLeft
        anchors.topMargin: (root.landscape ? 12 : 16) + root.safeTop

        Text {
            color: Theme.ink
            font.pixelSize: 16
            font.weight: Font.Medium
            text: qsTr("Score")
        }

        PixelNumber {
            objectName: "score"
            cell: 7
            text: root.game.score
        }
    }

    // The best score: at the top on the other side, or lying down under the score.
    Column {
        anchors.left: root.landscape ? parent.left : undefined
        anchors.right: root.landscape ? undefined : parent.right
        anchors.top: root.landscape ? scoreColumn.bottom : parent.top
        anchors.leftMargin: 16 + root.safeLeft
        anchors.rightMargin: 16 + root.safeRight
        anchors.topMargin: root.landscape ? 14 : 16 + root.safeTop
        spacing: 4

        Text {
            anchors.left: root.landscape ? parent.left : undefined
            anchors.right: root.landscape ? undefined : parent.right
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

            PixelNumber {
                id: bestText

                objectName: "bestScore"
                anchors.centerIn: parent
                color: Theme.surface
                cell: 4
                text: root.profile.best
            }
        }
    }
}
