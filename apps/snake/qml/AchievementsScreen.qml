// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// The twelve achievements and which of them this device has earned, one row each, in one panel. One row is always
// chosen: a tap chooses a row and the arrow keys move the choice (a gamepad sends arrow keys). The strip under the panel
// says what the chosen row takes to earn and whether it is earned. A row shows a tick when it is earned and a padlock
// when it is not, so the colour is never the only sign.
FocusScope {
    id: root

    required property GameViewModel game
    required property ProfileViewModel profile
    required property GameCenterDirector gameCenter

    // The row that is chosen, 0 to 11, and the row item that shows it (what the strip under the panel reads).
    property int current: 0
    readonly property var currentRow: rowRepeater.count > 0 ? rowRepeater.itemAt(root.current) : null

    // The player left the screen: the title shows next.
    signal closed

    onVisibleChanged: {
        if (visible) {
            root.current = 0;
            list.forceActiveFocus();
        }
    }

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Achievements")

    Keys.onEscapePressed: event => {
        root.closed();
        event.accepted = true;
    }

    PhonePage {
        id: page

        landscapeLayout: true

        ToyButton {
            objectName: "achievementsBackButton"
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
            objectName: "achievementsHeading"
            x: Theme.mirrored(page.landscape ? 66 : 74, width, page.pageWidth)
            // Shares the line with the Game Center button when there is one, and is set smaller to leave it room.
            maxWidth: root.gameCenter.available ? page.pageWidth - (page.landscape ? 66 : 74) - 15 - gameCenterButton.width - 8 : (page.landscape ? page.pageWidth - 66 - 15 : 300)
            y: page.landscape ? 8 : 26
            offset: 3
            pixelSize: page.landscape ? 30 : 34
            text: qsTr("Achievements")
        }

        // Game Center's own screen for the achievements, for a player who is signed in to it, at the trailing end of the
        // heading. The list is the one on this device either way.
        ToyButton {
            id: gameCenterButton

            objectName: "achievementsGameCenterButton"
            visible: root.gameCenter.available
            x: Theme.mirrored(page.pageWidth - 15 - width, width, page.pageWidth)
            y: page.landscape ? 12 : 32
            width: 130
            height: page.landscape ? 34 : 38
            radius: 10
            fontSize: 15
            text: qsTr("Game Center")
            onClicked: root.gameCenter.showAchievements()
        }

        Item {
            id: panel

            objectName: "achievementsPanel"
            x: 15
            y: page.landscape ? 54 : 92
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

                // Upright the twelve rows are one column; lying down they are two of six, the first six on the leading side.
                readonly property int rowHeight: page.landscape ? 33 : 46

                width: parent.width
                height: (page.landscape ? 6 : 12) * rowHeight + 20
                radius: 18
                color: Theme.surface
                border.width: 4
                border.color: Theme.ink

                Grid {
                    id: list

                    objectName: "achievementList"
                    x: 10
                    y: 10
                    width: parent.width - 20
                    rows: page.landscape ? 6 : 12
                    flow: Grid.TopToBottom
                    columnSpacing: 12
                    focus: true
                    activeFocusOnTab: true

                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Achievements")

                    // Up and down move through the twelve in order. Lying down, the other two arrows cross to the other
                    // column: the arrow that points to the other column, which is the right one unless the page is mirrored.
                    Keys.onPressed: event => {
                        let step = 0;
                        if (event.key === Qt.Key_Up)
                            step = -1;
                        else if (event.key === Qt.Key_Down)
                            step = 1;
                        else if (page.landscape && (event.key === Qt.Key_Left || event.key === Qt.Key_Right))
                            step = (event.key === Qt.Key_Right) !== Theme.rightToLeft ? 6 : -6;
                        else
                            return;
                        const next = root.current + step;
                        if (next >= 0 && next < 12)
                            root.current = next;
                        event.accepted = true;
                    }

                    Repeater {
                        id: rowRepeater

                        model: root.profile.achievements

                        Rectangle {
                            id: line

                            required property int index
                            required property int number
                            required property string name
                            required property string description
                            required property bool earned
                            required property string progress

                            readonly property bool chosen: root.current === line.index
                            readonly property string status: line.earned ? qsTr("Earned") : qsTr("Not earned yet")

                            objectName: "achievement" + line.number
                            width: page.landscape ? (list.width - list.columnSpacing) / 2 : list.width
                            height: board.rowHeight
                            radius: 10
                            color: line.chosen ? Theme.background : "transparent"
                            border.width: line.chosen ? 3 : 0
                            border.color: Theme.ink

                            // A sentence ends with a full stop, whether or not the text already has one.
                            function sentence(text: string): string {
                                return /[.!?。！？]$/.test(text) ? text : text + ".";
                            }

                            // What a screen reader says for the row, so that it needs no tap.
                            Accessible.role: Accessible.ListItem
                            Accessible.name: line.name + ". " + line.sentence(line.description) + " " + line.sentence(line.status) + (line.progress === "" ? "" : " " + line.progress)
                            Accessible.selected: line.chosen

                            // The tile: a tick when the achievement is earned, a padlock when it is not.
                            Rectangle {
                                id: tile

                                objectName: "achievementTile"
                                anchors.left: parent.left
                                anchors.leftMargin: page.landscape ? 4 : 7
                                anchors.verticalCenter: parent.verticalCenter
                                width: page.landscape ? 28 : 32
                                height: width
                                radius: 6
                                color: line.earned ? Theme.accent : Theme.surface
                                border.width: page.landscape ? 2 : 3
                                border.color: Theme.ink

                                // The tick, as the mode cards draw it: five squares.
                                Item {
                                    objectName: "achievementTick"
                                    visible: line.earned
                                    anchors.centerIn: parent
                                    width: 22
                                    height: 22

                                    Repeater {
                                        model: [[2, 10], [6, 14], [10, 10], [14, 6], [18, 2]]

                                        Rectangle {
                                            required property var modelData

                                            x: modelData[0]
                                            y: modelData[1]
                                            width: 4
                                            height: 4
                                            color: Theme.ink
                                        }
                                    }
                                }

                                // The padlock, as the mode cards draw it.
                                Item {
                                    objectName: "achievementPadlock"
                                    visible: !line.earned
                                    anchors.centerIn: parent
                                    width: 20
                                    height: 22

                                    Repeater {
                                        model: [[5, 2, 10, 3], [3, 5, 3, 6], [14, 5, 3, 6], [1, 10, 18, 11]]

                                        Rectangle {
                                            required property var modelData

                                            x: modelData[0]
                                            y: modelData[1]
                                            width: modelData[2]
                                            height: modelData[3]
                                            color: Theme.ink
                                        }
                                    }
                                }
                            }

                            // The count of a counted achievement, at the trailing end. It has no letters, so it reads from left
                            // to right in every language, and only its place mirrors.
                            Text {
                                id: progressText

                                objectName: "achievementProgress"
                                visible: line.progress !== ""
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.ink
                                font.pixelSize: page.landscape ? 13 : 15
                                font.weight: Font.Bold
                                text: line.progress
                                Accessible.ignored: true
                            }

                            // The name takes what the tile and the count leave, and is set smaller when it is too long.
                            Text {
                                objectName: "achievementName"
                                anchors.left: tile.right
                                anchors.leftMargin: 10
                                anchors.right: progressText.visible ? progressText.left : parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignLeft
                                color: Theme.ink
                                font.pixelSize: page.landscape ? 15 : 17
                                font.weight: Font.DemiBold
                                fontSizeMode: Text.HorizontalFit
                                minimumPixelSize: 9
                                text: line.name
                                Accessible.ignored: true
                            }

                            TapHandler {
                                onTapped: {
                                    UiEvents.buttonPressed();
                                    root.current = line.index;
                                    list.forceActiveFocus();
                                }
                            }
                        }
                    }
                }
            }
        }

        // What the chosen row takes, and whether it is earned.
        Item {
            id: detail

            objectName: "achievementDetail"
            x: 15
            y: panel.y + panel.height + (page.landscape ? 8 : 16)
            width: panel.width
            height: page.landscape ? 62 : 104

            Rectangle {
                y: 4
                width: parent.width
                height: parent.height
                radius: 14
                color: Theme.shadow
            }

            Rectangle {
                width: parent.width
                height: parent.height
                radius: 14
                color: Theme.surface
                border.width: 3
                border.color: Theme.ink

                Text {
                    objectName: "achievementDescription"
                    x: 14
                    y: 8
                    width: parent.width - 28
                    height: parent.height - 16 - statusText.height - 2
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                    fontSizeMode: Text.Fit
                    minimumPixelSize: 9
                    color: Theme.ink
                    font.pixelSize: page.landscape ? 15 : 18
                    font.weight: Font.Medium
                    text: root.currentRow ? root.currentRow.description : ""
                    Accessible.ignored: true
                }

                Text {
                    id: statusText

                    objectName: "achievementStatus"
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    horizontalAlignment: Text.AlignLeft
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: 9
                    color: Theme.ink
                    font.pixelSize: page.landscape ? 14 : 16
                    font.weight: Font.Bold
                    text: root.currentRow ? root.currentRow.status : ""
                    Accessible.ignored: true
                }
            }
        }
    }
}
