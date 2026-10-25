// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// A dashed outline for what is not available. The dashes run along the straight edges and stop short of the corners.
Item {
    id: root

    property color color: Theme.ink
    property int thickness: 3
    property int dash: 8
    property int gap: 5
    property int inset: 12

    readonly property int stride: dash + gap
    readonly property int across: Math.max(0, Math.floor((width - 2 * inset + gap) / stride))
    readonly property int down: Math.max(0, Math.floor((height - 2 * inset + gap) / stride))

    Repeater {
        model: root.across

        Item {
            id: column

            required property int index

            Rectangle {
                x: root.inset + column.index * root.stride
                width: root.dash
                height: root.thickness
                color: root.color
            }

            Rectangle {
                x: root.inset + column.index * root.stride
                y: root.height - root.thickness
                width: root.dash
                height: root.thickness
                color: root.color
            }
        }
    }

    Repeater {
        model: root.down

        Item {
            id: row

            required property int index

            Rectangle {
                y: root.inset + row.index * root.stride
                width: root.thickness
                height: root.dash
                color: root.color
            }

            Rectangle {
                x: root.width - root.thickness
                y: root.inset + row.index * root.stride
                width: root.thickness
                height: root.dash
                color: root.color
            }
        }
    }
}
