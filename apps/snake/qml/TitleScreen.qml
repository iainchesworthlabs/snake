// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// The title and the choice of mode. Play starts a game in the chosen mode.
FocusScope {
    id: root

    required property GameViewModel game
    required property ProfileViewModel profile

    // Each time the screen shows, the keyboard starts on its main button.
    onVisibleChanged: {
        if (visible)
            playButton.forceActiveFocus();
    }

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Snake")

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.game.start();
            event.accepted = true;
        }
    }

    PhonePage {
        ShadowText {
            objectName: "wordmark"
            anchors.horizontalCenter: parent.horizontalCenter
            y: 28
            offset: 5
            pixelSize: 76
            text: qsTr("Snake")
        }

        Column {
            x: 15
            y: 150
            width: 360
            spacing: 10

            Text {
                color: Theme.ink
                font.pixelSize: 17
                font.weight: Font.DemiBold
                text: qsTr("Choose a mode")
            }

            Column {
                width: parent.width
                spacing: 8

                Repeater {
                    model: root.profile.modes

                    ModeCard {
                        id: card

                        objectName: "modeCard" + card.entry
                        width: parent.width
                        chosen: card.selectable && root.game.mode === card.gameMode
                        onPicked: root.game.mode = card.gameMode
                    }
                }
            }
        }

        ToyButton {
            id: playButton

            objectName: "playButton"
            x: 15
            y: 750
            width: 360
            height: 62
            focus: true
            kind: "primary"
            fontSize: 28
            drop: 6
            radius: 14
            text: qsTr("Play")
            onClicked: root.game.start()
        }
    }
}
