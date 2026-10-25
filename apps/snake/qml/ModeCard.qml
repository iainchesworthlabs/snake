// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// One mode of the title screen. A mode the game can start is a button: the chosen one is coral with a tick. A mode
// that is locked, or open but not in the game yet, has a dashed outline and cannot be chosen. The arrow keys move the
// focus from one mode the game can start to the next, and a mode takes the choice when it gets the focus.
Item {
    id: root

    required property int entry
    required property string name
    required property string detail
    required property bool locked
    required property bool selectable
    required property int gameMode
    required property bool chosen

    signal picked

    height: 60
    activeFocusOnTab: selectable
    Accessible.role: Accessible.Button
    Accessible.name: name + ". " + detail
    Accessible.selected: chosen

    onActiveFocusChanged: {
        if (activeFocus && selectable)
            picked();
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            root.nextItemInFocusChain(event.key === Qt.Key_Down).forceActiveFocus(Qt.TabFocusReason);
            event.accepted = true;
        }
    }

    Rectangle {
        visible: root.selectable
        y: 4
        width: root.width
        height: root.height
        radius: 12
        color: Theme.shadow
    }

    Rectangle {
        id: face

        width: root.width
        height: root.height
        radius: 12
        color: !root.selectable ? "transparent" : root.chosen ? Theme.accent : Theme.surface
        border.width: root.selectable ? 3 : 0
        border.color: Theme.ink

        Column {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                color: Theme.ink
                font.pixelSize: 20
                font.weight: Font.Bold
                text: root.name
            }

            Text {
                color: Theme.ink
                font.pixelSize: 14
                font.weight: Font.Medium
                text: root.detail
            }
        }

        // The tick of the chosen mode, as the design draws it: five squares.
        Item {
            visible: root.selectable && root.chosen
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
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

        // The padlock of a locked mode.
        Item {
            visible: root.locked
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
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

    DashedFrame {
        visible: !root.selectable
        anchors.fill: parent
        color: Qt.rgba(Theme.ink.r, Theme.ink.g, Theme.ink.b, 0.55)
    }

    Rectangle {
        visible: root.activeFocus
        x: -6
        y: -6
        width: root.width + 12
        height: root.height + 16
        radius: 18
        color: "transparent"
        border.width: 3
        border.color: Theme.accent
    }

    TapHandler {
        enabled: root.selectable
        onTapped: {
            UiEvents.buttonPressed();
            root.picked();
            root.forceActiveFocus(Qt.MouseFocusReason);
        }
    }
}
