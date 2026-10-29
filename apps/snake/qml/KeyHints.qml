// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// What the keys do, for a computer with a keyboard: a card at the side of the board, in the design's style, with a chip for each
// key and what it does. The names of the keys that have a name in the language (Space, Esc, Enter) are translated, as the
// keyboards of the language have them. In Hot potato Enter sheds. When a gamepad is plugged in, its buttons are shown beside the keys.
Item {
    id: root

    // Hot potato: show the key that sheds.
    property bool shed: false
    // A gamepad is plugged in: show its buttons too.
    property bool gamepad: false

    // Each line: the keys, the buttons of the gamepad for it, and what it does.
    readonly property var lines: {
        const all = [
            {
                "keys": ["W", "A", "S", "D"],
                "pad": ["D-pad"],
                "what": qsTr("Steer (or the arrow keys)")
            },
            {
                "keys": [qsTr("Space")],
                "pad": ["Start"],
                "what": qsTr("Pause")
            },
            {
                "keys": [qsTr("Esc")],
                "pad": ["B"],
                "what": qsTr("Menu")
            }
        ];
        if (shed)
            all.push({
                "keys": [qsTr("Enter")],
                "pad": ["A"],
                "what": qsTr("Shed the tail")
            });
        return all;
    }

    implicitWidth: 240
    implicitHeight: card.height + 6

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Controls")

    Rectangle {
        y: 6
        width: parent.width
        height: card.height
        radius: 18
        color: Theme.shadow
    }

    Rectangle {
        id: card

        width: parent.width
        height: column.height + 28
        radius: 18
        color: Theme.surface
        border.width: 4
        border.color: Theme.ink

        Column {
            id: column

            x: 16
            y: 14
            width: parent.width - 32
            spacing: 12

            Text {
                width: parent.width
                color: Theme.ink
                font.pixelSize: 17
                font.weight: Font.Bold
                text: qsTr("Controls")
            }

            Repeater {
                model: root.lines

                Column {
                    id: line

                    required property var modelData

                    width: column.width
                    spacing: 4

                    Flow {
                        width: parent.width
                        spacing: 5

                        Repeater {
                            model: line.modelData.keys

                            Rectangle {
                                id: chip

                                required property string modelData

                                width: Math.max(24, label.implicitWidth + 14)
                                height: 24
                                radius: 6
                                color: Theme.background
                                border.width: 2
                                border.color: Theme.ink

                                Text {
                                    id: label

                                    anchors.centerIn: parent
                                    color: Theme.ink
                                    font.pixelSize: 13
                                    font.weight: Font.Bold
                                    text: chip.modelData
                                }
                            }
                        }

                        // The gamepad's buttons for it, in coral.
                        Repeater {
                            model: root.gamepad ? line.modelData.pad : []

                            Rectangle {
                                id: padChip

                                required property string modelData

                                width: Math.max(24, padLabel.implicitWidth + 14)
                                height: 24
                                radius: 12
                                color: Theme.accent
                                border.width: 2
                                border.color: Theme.ink

                                Text {
                                    id: padLabel

                                    anchors.centerIn: parent
                                    color: Theme.ink
                                    font.pixelSize: 13
                                    font.weight: Font.Bold
                                    text: padChip.modelData
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Theme.ink
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        text: line.modelData.what
                    }
                }
            }
        }
    }
}
