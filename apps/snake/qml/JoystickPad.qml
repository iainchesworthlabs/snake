// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// An on-screen joystick for a thumb: a cream plate with a coral knob that follows the finger and springs back to the middle
// when it lets go. The knob's place, as a fraction of the way it can travel, goes to the input controller, which turns it
// into a direction. Like the d-pad it sits under the board.
Item {
    id: root

    required property InputController input

    // The plate and the knob, and how far the middle of the knob can travel from the middle of the plate.
    readonly property int plateSize: 176
    readonly property int knobSize: 72
    readonly property real travel: (plateSize - knobSize) / 2

    // Where the knob is: pixels from the middle of the plate, right and down positive.
    property real knobX: 0
    property real knobY: 0

    implicitWidth: plateSize
    implicitHeight: plateSize + 8

    Accessible.role: Accessible.Slider
    Accessible.name: qsTr("Joystick")

    function follow(x, y) {
        let dx = x - plateSize / 2;
        let dy = y - plateSize / 2;
        const length = Math.hypot(dx, dy);
        if (length > travel) {
            dx *= travel / length;
            dy *= travel / length;
        }
        knobX = dx;
        knobY = dy;
        // Right and up are positive for the controller.
        input.joystickMoved(dx / travel, -dy / travel);
    }

    function letGo() {
        knobX = 0;
        knobY = 0;
        input.joystickReleased();
    }

    Rectangle {
        y: 8
        width: root.plateSize
        height: root.plateSize
        radius: 28
        color: Theme.shadow
    }

    Rectangle {
        id: plate

        width: root.plateSize
        height: root.plateSize
        radius: 28
        color: Theme.surface
        border.width: 4
        border.color: Theme.ink

        // The ring the knob can reach.
        Rectangle {
            anchors.centerIn: parent
            width: root.travel * 2 + 16
            height: width
            radius: width / 2
            color: "transparent"
            border.width: 2
            border.color: Theme.ink
            opacity: 0.25
        }

        Rectangle {
            objectName: "joystickKnob"
            x: plate.width / 2 - width / 2 + root.knobX
            y: plate.height / 2 - height / 2 + root.knobY
            width: root.knobSize
            height: root.knobSize
            radius: 20
            color: Theme.accent
            border.width: 4
            border.color: Theme.ink
        }

        MouseArea {
            objectName: "joystickArea"
            anchors.fill: parent
            preventStealing: true
            onPressed: mouse => root.follow(mouse.x, mouse.y)
            onPositionChanged: mouse => root.follow(mouse.x, mouse.y)
            onReleased: root.letGo()
            onCanceled: root.letGo()
        }
    }
}
