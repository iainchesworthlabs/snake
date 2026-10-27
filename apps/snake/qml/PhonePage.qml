// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The 390 by 844 page of the design, scaled to fit its parent and centred in it, so the phone layout is used at any
// window size. Children are placed on the page.
Item {
    id: root

    readonly property int pageWidth: 390
    readonly property int pageHeight: 844

    // The parts of the screen that a phone covers (the status bar, the camera cut-out, the home indicator, the rounded corners).
    // Zero on a computer. The page fits inside what is left.
    // An item over the whole screen that this page sits on (not under the page's scaling), to read the margins from.
    Item {
        id: screen

        parent: root.parent
        anchors.fill: parent
    }

    readonly property real insetTop: screen.SafeArea.margins.top
    readonly property real insetBottom: screen.SafeArea.margins.bottom
    readonly property real insetLeft: screen.SafeArea.margins.left
    readonly property real insetRight: screen.SafeArea.margins.right
    readonly property real availableWidth: root.parent.width - insetLeft - insetRight
    readonly property real availableHeight: root.parent.height - insetTop - insetBottom

    width: pageWidth
    height: pageHeight
    x: insetLeft + (availableWidth - pageWidth) / 2
    y: insetTop + (availableHeight - pageHeight) / 2
    scale: Math.min(availableWidth / pageWidth, availableHeight / pageHeight)

    // In a language that reads right to left the page is mirrored: anchors, rows and the alignment of text swap sides.
    // The board and the names, which are Latin letters, are not mirrored (see the places that turn it off).
    LayoutMirroring.enabled: Theme.rightToLeft
    LayoutMirroring.childrenInherit: true
}
