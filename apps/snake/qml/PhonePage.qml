// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The page of the design, scaled to fit its parent and centred in it, so the phone layout is used at any window size.
// Children are placed on the page.
//
// The page is 390 by 844, upright. A screen that has a landscape layout sets `landscapeLayout`, and while its parent is
// wider than it is tall the page lies down: 360 high and as wide as the parent's shape makes it (600 to 900), so the
// screen lays out two columns and fills the window instead of being a small portrait page in the middle of it. The
// screen reads `landscape` to place its parts.
Item {
    id: root

    // True for a screen that has a landscape layout.
    property bool landscapeLayout: false

    readonly property bool landscape: landscapeLayout && root.parent.width > root.parent.height

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

    readonly property real pageHeight: landscape ? 360 : 844
    readonly property real pageWidth: landscape ? Math.max(600, Math.min(900, 360 * availableWidth / availableHeight)) : 390

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
