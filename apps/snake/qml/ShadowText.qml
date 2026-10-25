// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// A heading in the ink colour with the hard coral shadow of the design. A heading that is wider than `maxWidth` in some
// language is set smaller, down to a third of its size, until it fits.
Item {
    id: root

    property alias text: front.text
    property alias pixelSize: front.font.pixelSize
    property int offset: 4
    property int maxWidth: 360

    implicitWidth: front.contentWidth + offset
    implicitHeight: front.implicitHeight + offset

    Text {
        x: root.offset
        y: root.offset
        color: Theme.accent
        font.family: front.font.family
        font.weight: front.font.weight
        font.pixelSize: front.fontInfo.pixelSize
        text: front.text
    }

    Text {
        id: front

        // As wide as the words, or as wide as there is room for and the words set smaller to fit.
        width: Math.min(root.maxWidth, implicitWidth)
        color: Theme.ink
        font.weight: Font.Bold
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: Math.max(10, font.pixelSize / 3)
    }
}
