// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// A number drawn from squares on a 3 by 5 grid, each square `cell` pixels wide, so that every digit is the same few pixels at any
// size and none can be mistaken for another. The font of the game draws a 2, a 3 and a 5 as an 8, an 8 and an S at the sizes
// a score is set in. Only digits are drawn; any other character leaves a gap. `text` is what is shown, and what a reader of
// the screen hears.
Item {
    id: root

    property string text
    // The side of one square, in pixels. A whole number keeps the edges sharp.
    property int cell: 6
    property color color: Theme.ink

    // The squares of each digit, row by row from the top, left to right, 1 for filled.
    readonly property var glyphs: ({
            "0": "111101101101111",
            "1": "010110010010111",
            "2": "111001111100111",
            "3": "111001111001111",
            "4": "101101111001001",
            "5": "111100111001111",
            "6": "111100111101111",
            "7": "111001001010010",
            "8": "111101111101111",
            "9": "111101111001111"
        })

    implicitWidth: text.length > 0 ? text.length * 4 * cell - cell : 0
    implicitHeight: 5 * cell

    Accessible.role: Accessible.StaticText
    Accessible.name: root.text

    Repeater {
        model: root.text.length

        Item {
            id: digit

            required property int index
            readonly property string pattern: root.glyphs[root.text.charAt(digit.index)] ?? ""

            x: digit.index * 4 * root.cell
            width: 3 * root.cell
            height: 5 * root.cell

            Repeater {
                model: digit.pattern.length

                Rectangle {
                    id: square

                    required property int index

                    visible: digit.pattern.charAt(square.index) === "1"
                    x: (square.index % 3) * root.cell
                    y: Math.floor(square.index / 3) * root.cell
                    width: root.cell
                    height: root.cell
                    color: root.color
                }
            }
        }
    }
}
