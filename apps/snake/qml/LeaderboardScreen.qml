// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// The top 10 of a mode on this device, with a tab for each mode. The line saved last is coral.
FocusScope {
    id: root

    required property GameViewModel game
    required property ProfileViewModel profile

    // The player left the screen: the title shows next.
    signal closed

    onVisibleChanged: {
        if (visible)
            playButton.forceActiveFocus();
    }

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Leaderboard")

    Keys.onEscapePressed: event => {
        root.closed();
        event.accepted = true;
    }

    PhonePage {
        id: page

        landscapeLayout: true

        ToyButton {
            objectName: "backButton"
            x: Theme.mirrored(15, width, page.pageWidth)
            y: page.landscape ? 10 : 28
            width: page.landscape ? 42 : 46
            height: page.landscape ? 42 : 46
            radius: 10
            text: LayoutMirroring.enabled ? ">" : "<"
            fontSize: 24
            Accessible.name: qsTr("Back")
            onClicked: root.closed()
        }

        ShadowText {
            x: Theme.mirrored(page.landscape ? 66 : 74, width, page.pageWidth)
            maxWidth: page.landscape ? page.pageWidth - 66 - 15 : 300
            y: page.landscape ? 8 : 26
            offset: 3
            pixelSize: page.landscape ? 30 : 34
            text: qsTr("Leaderboard")
        }

        // The tabs, one for each mode: two rows of two under the title, or one row of four under it lying down.
        Grid {
            id: tabs

            x: 15
            y: page.landscape ? 54 : 92
            columns: page.landscape ? 4 : 2
            spacing: 8

            Repeater {
                model: [
                    {
                        "id": "easy",
                        "mode": GameViewModel.Mode.Easy,
                        "name": qsTr("Easy")
                    },
                    {
                        "id": "classic",
                        "mode": GameViewModel.Mode.Classic,
                        "name": qsTr("Classic")
                    },
                    {
                        "id": "portals",
                        "mode": GameViewModel.Mode.Portals,
                        "name": qsTr("Portals")
                    },
                    {
                        "id": "hotPotato",
                        "mode": GameViewModel.Mode.HotPotato,
                        "name": qsTr("Hot potato")
                    }
                ]

                ToyButton {
                    id: tab

                    required property var modelData

                    objectName: tab.modelData.id + "Tab"
                    width: page.landscape ? (page.pageWidth - 30 - 3 * tabs.spacing) / 4 : 176
                    height: page.landscape ? 38 : 46
                    kind: root.profile.boardMode === tab.modelData.mode ? "primary" : "secondary"
                    text: tab.modelData.name
                    onClicked: root.profile.boardMode = tab.modelData.mode
                }
            }
        }

        Text {
            visible: !page.landscape
            x: Theme.mirrored(15, width, 390)
            y: 204
            color: Theme.ink
            font.pixelSize: 15
            font.weight: Font.Medium
            text: qsTr("Top 10 on this device")
        }

        Item {
            x: 15
            y: page.landscape ? 100 : 236
            width: page.landscape ? page.pageWidth - 30 : 360
            height: board.height + 6

            Rectangle {
                y: 6
                width: parent.width
                height: board.height
                radius: 18
                color: Theme.shadow
            }

            Rectangle {
                id: board

                width: parent.width
                // Upright the ten lines are one column; lying down they are two of five, the first five on the leading side.
                readonly property int lineHeight: page.landscape ? 33 : 48
                height: (page.landscape ? 5 : 10) * lineHeight + 20
                radius: 18
                color: Theme.surface
                border.width: 4
                border.color: Theme.ink

                Text {
                    objectName: "emptyBoard"
                    anchors.centerIn: parent
                    visible: lines.count === 0
                    color: Theme.ink
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    text: qsTr("No scores yet.")
                }

                Grid {
                    id: grid

                    x: 10
                    y: 10
                    width: parent.width - 20
                    rows: page.landscape ? 5 : 10
                    flow: Grid.TopToBottom
                    columnSpacing: 12

                    Repeater {
                        id: lines

                        model: root.profile.leaderboard

                        Rectangle {
                            id: line

                            required property int place
                            required property string name
                            required property string score
                            required property bool latest

                            objectName: "line" + line.place
                            width: page.landscape ? (grid.width - grid.columnSpacing) / 2 : grid.width
                            height: board.lineHeight
                            radius: 10
                            color: line.latest ? Theme.accent : "transparent"

                            // The first three places have a coloured tile.
                            Rectangle {
                                x: Theme.mirrored(6, width, line.width)
                                anchors.verticalCenter: parent.verticalCenter
                                width: 32
                                height: 32
                                radius: 6
                                color: line.place === 1 ? Theme.medalGold : line.place === 2 ? Theme.medalSilver : line.place === 3 ? Theme.medalBronze : "transparent"
                                border.width: line.place <= 3 ? 3 : 0
                                border.color: Theme.ink

                                Text {
                                    anchors.centerIn: parent
                                    color: Theme.ink
                                    font.pixelSize: 15
                                    font.weight: Font.Bold
                                    text: line.place
                                }
                            }

                            Text {
                                x: Theme.mirrored(52, width, line.width)
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.ink
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                text: line.name
                            }

                            PixelNumber {
                                objectName: "lineScore"
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                cell: 3
                                text: line.score
                            }
                        }
                    }
                }
            }
        }

        ToyButton {
            id: playButton

            objectName: "leaderboardPlayButton"
            x: page.landscape ? Theme.mirrored((page.pageWidth - width) / 2, width, page.pageWidth) : 15
            y: page.landscape ? 296 : 754
            width: page.landscape ? 320 : 360
            height: page.landscape ? 50 : 56
            focus: true
            kind: "primary"
            fontSize: page.landscape ? 24 : 26
            drop: 6
            radius: 14
            text: qsTr("Play")
            onClicked: root.game.restart()
        }
    }
}
