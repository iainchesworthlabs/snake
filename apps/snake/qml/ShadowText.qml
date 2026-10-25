// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// A heading in the ink colour with the hard coral shadow of the design.
Item {
    id: root

    property alias text: front.text
    property alias pixelSize: front.font.pixelSize
    property int offset: 4

    implicitWidth: front.implicitWidth + offset
    implicitHeight: front.implicitHeight + offset

    Text {
        x: root.offset
        y: root.offset
        color: Theme.accent
        font: front.font
        text: front.text
    }

    Text {
        id: front

        color: Theme.ink
        font.weight: Font.Bold
    }
}
