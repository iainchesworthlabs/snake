// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// The title and the choice of mode. Play starts a game in the chosen mode.
FocusScope {
    id: root

    required property GameViewModel game
    required property ProfileViewModel profile

    signal leaderboardRequested
    signal settingsRequested

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
        id: page

        landscapeLayout: true

        // Lying down the page has two columns: the name, the attract mode, Play and the other screens on the leading side, and the
        // choice of mode on the other. In a language read from right to left the columns swap.
        readonly property real margin: 16
        readonly property real columnWidth: (pageWidth - 3 * margin) / 2
        readonly property real leadingX: Theme.mirrored(margin, columnWidth, pageWidth)
        readonly property real trailingX: Theme.mirrored(2 * margin + columnWidth, columnWidth, pageWidth)

        ShadowText {
            objectName: "wordmark"
            x: (page.landscape ? page.leadingX + page.columnWidth / 2 : page.pageWidth / 2) - width / 2
            y: page.landscape ? 6 : 28
            offset: 5
            maxWidth: page.landscape ? page.columnWidth : 360
            // The pixel font has Latin letters only; the name in another script is drawn in a font that is taller, so
            // it is set smaller to stay above the preview.
            pixelSize: /^[\u0000-\u024F]*$/.test(text) ? (page.landscape ? 48 : 76) : (page.landscape ? 36 : 54)
            text: qsTr("Snake")
        }

        // The attract mode: the game playing itself on a board that scrolls to keep the head in view, in the frame of
        // the design. It plays only while the title shows.
        GameViewModel {
            id: demo

            objectName: "demoGame"
            autoplay: true
        }

        FrameAnimation {
            running: root.visible
            onTriggered: demo.advance(frameTime)
        }

        Item {
            objectName: "attractPreview"
            x: page.landscape ? page.leadingX : 15
            y: page.landscape ? 80 : 118
            width: page.landscape ? page.columnWidth : 360
            height: page.landscape ? 132 : 168

            Rectangle {
                y: 6
                width: parent.width
                height: parent.height
                radius: 18
                color: Theme.shadow
            }

            Rectangle {
                width: parent.width
                height: parent.height
                radius: 18
                color: Theme.ink
            }

            Item {
                x: 6
                y: 6
                width: parent.width - 12
                height: parent.height - 12
                clip: true

                BoardItem {
                    id: demoBoard

                    objectName: "demoBoard"
                    width: parent.width
                    height: parent.width
                    // The head's row in the middle of the window, as far as the board goes.
                    y: Math.max(parent.height - height, Math.min(0, parent.height / 2 - (demo.headRow + 0.5) * height / 20))
                    game: demo

                    Behavior on y {
                        NumberAnimation {
                            duration: 200
                        }
                    }
                }
            }
        }

        Column {
            x: page.landscape ? page.trailingX : 15
            y: page.landscape ? 14 : 300
            width: page.landscape ? page.columnWidth : 360
            spacing: 10

            Text {
                width: parent.width
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
            x: page.landscape ? page.leadingX : 15
            y: page.landscape ? 224 : 686
            width: page.landscape ? page.columnWidth : 360
            height: page.landscape ? 56 : 62
            focus: true
            kind: "primary"
            fontSize: page.landscape ? 26 : 28
            drop: 6
            radius: 14
            text: qsTr("Play")
            onClicked: root.game.start()
        }

        ToyButton {
            objectName: "leaderboardButton"
            x: page.landscape ? Theme.mirrored(page.margin, width, page.pageWidth) : Theme.mirrored(15, width, 390)
            y: page.landscape ? 296 : 762
            width: page.landscape ? page.columnWidth - 56 : 300
            height: page.landscape ? 48 : 50
            text: qsTr("Leaderboard")
            onClicked: root.leaderboardRequested()
        }

        ToyButton {
            objectName: "settingsButton"
            x: page.landscape ? Theme.mirrored(page.margin + page.columnWidth - 48, width, page.pageWidth) : Theme.mirrored(325, width, 390)
            y: page.landscape ? 296 : 762
            width: page.landscape ? 48 : 50
            height: page.landscape ? 48 : 50
            Accessible.name: qsTr("Settings")
            onClicked: root.settingsRequested()

            // The sliders of the design: two bars with a square on each.
            Item {
                anchors.centerIn: parent
                width: 28
                height: 22

                Repeater {
                    model: 2

                    Item {
                        id: slider

                        required property int index

                        y: slider.index * 14
                        width: 28
                        height: 8

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: 3
                            color: Theme.ink
                        }

                        Rectangle {
                            x: slider.index === 0 ? 4 : 16
                            width: 8
                            height: 8
                            color: Theme.ink
                        }
                    }
                }
            }
        }
    }
}
