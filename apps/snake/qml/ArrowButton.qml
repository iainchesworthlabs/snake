// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// A button with an arrow on it, for the d-pad: a cream square on a hard shadow, like the other buttons, with a pixel arrow
// drawn from rectangles pointing the way `heading` says. The face is 64 pixels square, over the 44 that a finger needs.
ToyButton {
    id: root

    // 0 up, 1 down, 2 left, 3 right: the order of InputController.Heading without None.
    property int arrow: 0

    width: 64
    height: 64
    radius: 14
    activeFocusOnTab: false

    // The arrow: a head of three rows and a shaft, as a pixel triangle. It is drawn pointing up and turned.
    Item {
        anchors.centerIn: parent
        width: 28
        height: 28
        rotation: [0, 180, 270, 90][root.arrow]

        Repeater {
            // Rows from the point: the widths of the head, then the shaft.
            model: [[12, 4, 4], [8, 8, 4], [4, 12, 4], [0, 20, 4], [10, 8, 4], [10, 8, 4]]

            Rectangle {
                id: row

                required property var modelData
                required property int index

                x: modelData[0] + 4 - 2
                y: row.index * 4 + 2
                width: modelData[1]
                height: 4
                color: Theme.ink
            }
        }
    }
}
