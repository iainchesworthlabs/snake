// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The sign that swiping steers: four arrows round a dot, faint, in the place where the other ways of steering have their
// controls. A swipe works anywhere on the screen, so there is nothing to touch here; it only tells the player which way
// the game is steered. It has no words, so it reads in every language.
Item {
    id: root

    implicitWidth: 72
    implicitHeight: 72

    opacity: 0.4

    Accessible.role: Accessible.Indicator
    Accessible.name: qsTr("Swipe")

    // The arrow at the top, then the same turned a quarter, a half and three quarters round the middle.
    Repeater {
        model: 4

        Item {
            id: arrow

            required property int index

            anchors.fill: parent
            rotation: arrow.index * 90

            // A chevron of two bars that meet at the point.
            Item {
                x: (parent.width - width) / 2
                y: 0
                width: 24
                height: 14

                Rectangle {
                    x: 7 - width / 2
                    y: 7 - height / 2
                    width: 18
                    height: 4
                    radius: 1
                    rotation: -45
                    color: Theme.ink
                }

                Rectangle {
                    x: 17 - width / 2
                    y: 7 - height / 2
                    width: 18
                    height: 4
                    radius: 1
                    rotation: 45
                    color: Theme.ink
                }
            }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 8
        height: 8
        radius: 2
        color: Theme.ink
    }
}
