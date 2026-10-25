// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The 390 by 844 page of the design, scaled to fit its parent and centred in it, so the phone layout is used at any
// window size. Children are placed on the page.
Item {
    id: root

    readonly property int pageWidth: 390
    readonly property int pageHeight: 844

    width: pageWidth
    height: pageHeight
    x: (root.parent.width - pageWidth) / 2
    y: (root.parent.height - pageHeight) / 2
    scale: Math.min(root.parent.width / pageWidth, root.parent.height / pageHeight)

    // In a language that reads right to left the page is mirrored: anchors, rows and the alignment of text swap sides.
    // The board and the names, which are Latin letters, are not mirrored (see the places that turn it off).
    LayoutMirroring.enabled: Theme.rightToLeft
    LayoutMirroring.childrenInherit: true
}
