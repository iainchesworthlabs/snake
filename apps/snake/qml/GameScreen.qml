// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The game: the score and the best score above the board. The keys go to the game. This layout is a stand-in for the
// design's phone and desktop pages.
FocusScope {
    id: root

    required property GameViewModel game
    required property ProfileViewModel profile
    required property EffectsModel effects

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Game")

    Keys.onPressed: event => {
        event.accepted = root.game.handleKey(event.key, event.isAutoRepeat);
    }

    BoardItem {
        id: board

        objectName: "board"
        anchors.fill: parent
        anchors.margins: 16
        anchors.topMargin: 64
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
        anchors.margins: 16

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
        anchors.margins: 16
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

                anchors.centerIn: parent
                color: Theme.surface
                font.pixelSize: 24
                font.weight: Font.Bold
                text: root.profile.best
            }
        }
    }
}
