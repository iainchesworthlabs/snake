// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// A heading in the ink colour with the hard coral shadow of the design. A heading that is wider than `maxWidth` in some
// language is set smaller until it fits.
Item {
    id: root

    property alias text: front.text
    property int pixelSize: 20
    property int offset: 4
    property int maxWidth: 360

    // The size it is set at: the one asked for, or smaller when the words would be wider than `maxWidth`. Letters do not
    // grow in step with the size (they are drawn on whole pixels), so the first guess is measured and corrected once.
    readonly property int firstPixelSize: natural.width > root.maxWidth ? Math.max(10, Math.floor(root.pixelSize * root.maxWidth / natural.width)) : root.pixelSize
    readonly property int fittedPixelSize: guess.width > root.maxWidth ? Math.max(10, Math.floor(root.firstPixelSize * root.maxWidth / guess.width) - 1) : root.firstPixelSize

    implicitWidth: front.implicitWidth + offset
    implicitHeight: front.implicitHeight + offset

    TextMetrics {
        id: natural

        font.weight: Font.Bold
        font.pixelSize: root.pixelSize
        text: front.text
    }

    TextMetrics {
        id: guess

        font.weight: Font.Bold
        font.pixelSize: root.firstPixelSize
        text: front.text
    }

    Text {
        x: root.offset
        y: root.offset
        color: Theme.accent
        // The whole font of the front, with the fallbacks for other scripts.
        font: front.font
        text: front.text
    }

    Text {
        id: front

        color: Theme.ink
        font.weight: Font.Bold
        font.pixelSize: root.fittedPixelSize
    }
}
