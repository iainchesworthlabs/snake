// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The four arrows of the d-pad, in a cross, under the board. A press turns the snake the way the arrow points.
Item {
    id: root

    required property InputController input

    implicitWidth: 3 * 64 + 2 * 8
    implicitHeight: 3 * 64 + 2 * 8

    ArrowButton {
        objectName: "dpadUp"
        x: 64 + 8
        arrow: 0
        Accessible.name: qsTr("Up")
        onClicked: root.input.steer(InputController.Heading.Up)
    }

    ArrowButton {
        objectName: "dpadLeft"
        y: 64 + 8
        arrow: 2
        Accessible.name: qsTr("Left")
        onClicked: root.input.steer(InputController.Heading.Left)
    }

    ArrowButton {
        objectName: "dpadRight"
        x: 2 * (64 + 8)
        y: 64 + 8
        arrow: 3
        Accessible.name: qsTr("Right")
        onClicked: root.input.steer(InputController.Heading.Right)
    }

    ArrowButton {
        objectName: "dpadDown"
        x: 64 + 8
        y: 2 * (64 + 8)
        arrow: 1
        Accessible.name: qsTr("Down")
        onClicked: root.input.steer(InputController.Heading.Down)
    }
}
