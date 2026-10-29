// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// A labelled volume of the Toybox Pixel style: the label on the left and, on the right, a minus button, a row of pips
// (one for each step, filled up to the level) and a plus button. The owner sets the level when it gets `stepped`, so the
// control always shows what the setting is. The keyboard reaches it with Tab or the up and down arrows, and the left and
// right arrows turn it down and up (the other way round when the page is mirrored, because the minus button is then on
// the right). The focus shows as a ring. A press sounds at the new level, so the player hears what they chose.
Item {
    id: root

    property string text
    // The level, 0 to maximum.
    property int value
    property int maximum: 10
    property int fontSize: 20

    // The player asked for `level`, which is inside 0 to maximum.
    signal stepped(int level)

    function step(delta) {
        const level = Math.max(0, Math.min(maximum, value + delta));
        if (level === value)
            return;
        stepped(level);
        UiEvents.buttonPressed();
    }

    implicitHeight: 44

    activeFocusOnTab: true
    Accessible.role: Accessible.Slider
    Accessible.name: text
    Accessible.description: qsTr("%1 of %2").arg(value).arg(maximum)
    Accessible.onIncreaseAction: root.step(1)
    Accessible.onDecreaseAction: root.step(-1)

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
            const right = event.key === Qt.Key_Right;
            root.step(right !== LayoutMirroring.enabled ? 1 : -1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            root.nextItemInFocusChain(event.key === Qt.Key_Down).forceActiveFocus(Qt.TabFocusReason);
            event.accepted = true;
        }
    }

    Text {
        anchors.left: parent.left
        anchors.right: controls.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: Text.AlignLeft
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 10
        color: Theme.ink
        font.pixelSize: root.fontSize
        font.weight: Font.DemiBold
        text: root.text
    }

    Row {
        id: controls

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        Rectangle {
            objectName: "volumeDown"
            width: 34
            height: 34
            radius: 10
            color: Theme.surface
            border.width: 3
            border.color: Theme.ink
            opacity: root.value > 0 ? 1 : 0.4

            Rectangle {
                anchors.centerIn: parent
                width: 14
                height: 4
                color: Theme.ink
            }

            TapHandler {
                onTapped: root.step(-1)
            }
        }

        Row {
            objectName: "volumePips"
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

            Repeater {
                model: root.maximum

                Rectangle {
                    required property int index

                    width: 8
                    height: 18
                    radius: 2
                    color: index < root.value ? Theme.accent : Theme.surface
                    border.width: 2
                    border.color: Theme.ink
                }
            }
        }

        Rectangle {
            objectName: "volumeUp"
            width: 34
            height: 34
            radius: 10
            color: Theme.surface
            border.width: 3
            border.color: Theme.ink
            opacity: root.value < root.maximum ? 1 : 0.4

            Rectangle {
                anchors.centerIn: parent
                width: 14
                height: 4
                color: Theme.ink
            }

            Rectangle {
                anchors.centerIn: parent
                width: 4
                height: 14
                color: Theme.ink
            }

            TapHandler {
                onTapped: root.step(1)
            }
        }
    }

    Rectangle {
        visible: root.activeFocus
        x: -6
        y: -4
        width: root.width + 12
        height: root.height + 8
        radius: 14
        color: "transparent"
        border.width: 3
        border.color: Theme.accent
    }
}
