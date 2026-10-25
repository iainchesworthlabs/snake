// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// Name entry for a score that made the top 10: the place it will take, the name typed so far in twelve slots, the
// names used last as shortcuts, and a grid of letters. The keyboard types straight into the name: letters and Space add
// to it, Backspace takes the last one off and Enter saves.
FocusScope {
    id: root

    required property ProfileViewModel profile

    // The score was saved: the leaderboard shows next.
    signal saved

    readonly property int slotCount: root.profile.nameLength
    readonly property string letters: "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

    // The keyboard stays on the screen itself, so that Space types a space and not a press of a button.
    onVisibleChanged: {
        if (visible)
            forceActiveFocus();
    }

    function save() {
        if (profile.canSave && profile.saveScore())
            saved();
    }

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("New high score")

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (!event.isAutoRepeat)
                root.save();
        } else if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) {
            root.profile.eraseCharacter();
        } else if (event.text.length === 1) {
            root.profile.typeCharacter(event.text);
        } else {
            return;
        }
        event.accepted = true;
    }

    PhonePage {
        ShadowText {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 22
            offset: 3
            pixelSize: 34
            text: qsTr("New high score!")
        }

        ShadowText {
            objectName: "pendingScore"
            anchors.horizontalCenter: parent.horizontalCenter
            y: 72
            offset: 4
            pixelSize: 62
            text: root.profile.pendingScore
        }

        // The place it will take, between its neighbours.
        Item {
            x: 15
            y: 153
            width: 360
            height: standing.height + 6

            Rectangle {
                y: 6
                width: parent.width
                height: standing.height
                radius: 18
                color: Theme.shadow
            }

            Rectangle {
                id: standing

                width: parent.width
                height: standingColumn.height + 20
                radius: 18
                color: Theme.surface
                border.width: 4
                border.color: Theme.ink

                Column {
                    id: standingColumn

                    x: 10
                    y: 10
                    width: parent.width - 20

                    Repeater {
                        model: root.profile.standing

                        Rectangle {
                            id: row

                            required property var modelData

                            objectName: "standing" + row.modelData.place
                            width: standingColumn.width
                            height: 36
                            radius: 10
                            color: row.modelData.mine ? Theme.accent : "transparent"

                            Text {
                                x: Theme.mirrored(8, width, row.width)
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.ink
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                text: row.modelData.place
                            }

                            Text {
                                x: Theme.mirrored(44, width, row.width)
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.ink
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                text: row.modelData.name
                            }

                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.ink
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                text: row.modelData.score
                            }
                        }
                    }
                }
            }
        }

        Text {
            x: Theme.mirrored(15, width, 390)
            y: 300
            color: Theme.ink
            font.pixelSize: 15
            font.weight: Font.Medium
            text: qsTr("Your name")
        }

        Text {
            objectName: "nameCount"
            anchors.right: parent.right
            anchors.rightMargin: 15
            y: 300
            color: Theme.ink
            font.pixelSize: 15
            font.weight: Font.Medium
            text: qsTr("%1 of %2").arg(root.profile.entryName.length).arg(root.slotCount)
        }

        // The twelve slots. The next free one is coral.
        Row {
            x: 15
            y: 326
            spacing: 5
            // The name is letters from A to Z, whatever the language: the first letter is on the left.
            LayoutMirroring.enabled: false
            LayoutMirroring.childrenInherit: true

            Repeater {
                model: root.slotCount

                Rectangle {
                    id: slot

                    required property int index
                    readonly property string letter: root.profile.entryName.charAt(slot.index)

                    objectName: "slot" + slot.index
                    width: (360 - 5 * (root.slotCount - 1)) / root.slotCount
                    height: 41
                    radius: 8
                    color: slot.index === root.profile.entryName.length ? Theme.accent : Theme.surface
                    border.width: 3
                    border.color: Theme.ink

                    Text {
                        anchors.centerIn: parent
                        color: Theme.ink
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        text: slot.letter
                    }
                }
            }
        }

        // The names used last, one tap each.
        Row {
            x: Theme.mirrored(15, width, 390)
            y: 386
            spacing: 8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.profile.recentNames.length > 0
                color: Theme.ink
                font.pixelSize: 15
                font.weight: Font.Medium
                text: qsTr("Recent")
            }

            Repeater {
                model: root.profile.recentNames

                ToyButton {
                    id: chip

                    required property int index
                    required property string modelData

                    objectName: "recentName" + chip.index
                    width: Math.max(70, chipText.implicitWidth + 28)
                    height: 40
                    activeFocusOnTab: false
                    fontSize: 16
                    drop: 3
                    text: chip.modelData
                    onClicked: root.profile.chooseRecent(chip.index)

                    // The width follows the text; the button's own text is the same size.
                    Text {
                        id: chipText

                        visible: false
                        font.pixelSize: chip.fontSize
                        font.weight: Font.Bold
                        text: chip.modelData
                    }
                }
            }
        }

        // The letters, seven to a row, then a space and a delete.
        Grid {
            x: 15
            y: 448
            columns: 7
            // A to Z from the left, as on a keyboard.
            LayoutMirroring.enabled: false
            LayoutMirroring.childrenInherit: true
            rowSpacing: 7
            columnSpacing: 6

            Repeater {
                model: root.letters.length

                LetterKey {
                    id: key

                    required property int index

                    objectName: "key" + root.letters.charAt(key.index)
                    width: 46
                    height: 46
                    text: root.letters.charAt(key.index)
                    onClicked: root.profile.typeCharacter(key.text)
                }
            }

            LetterKey {
                objectName: "keySpace"
                width: 46
                height: 46
                fontSize: 15
                text: qsTr("Spc")
                onClicked: root.profile.typeCharacter(" ")
            }

            LetterKey {
                objectName: "keyDelete"
                width: 46
                height: 46
                fontSize: 15
                text: qsTr("Del")
                onClicked: root.profile.eraseCharacter()
            }
        }

        ToyButton {
            objectName: "saveNameButton"
            x: 15
            y: 754
            width: 360
            height: 56
            enabled: root.profile.canSave
            activeFocusOnTab: false
            kind: "primary"
            fontSize: 26
            drop: 6
            radius: 14
            text: qsTr("Save score")
            onClicked: root.save()
        }
    }
}
