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

    // The arrow, on a 7 by 7 grid of 4 pixel squares: a head that widens from one square to seven, and a shaft of three. It is drawn
    // pointing up and turned a quarter at a time, which keeps every edge on the grid.
    Item {
        anchors.centerIn: parent
        width: 28
        height: 28
        rotation: [0, 180, 270, 90][root.arrow]

        Repeater {
            // Each row: the column it starts at, and how many squares wide it is.
            model: [[3, 1], [2, 3], [1, 5], [0, 7], [2, 3], [2, 3], [2, 3]]

            Rectangle {
                id: row

                required property var modelData
                required property int index

                x: row.modelData[0] * 4
                y: row.index * 4
                width: row.modelData[1] * 4
                height: 4
                color: Theme.ink
            }
        }
    }
}
