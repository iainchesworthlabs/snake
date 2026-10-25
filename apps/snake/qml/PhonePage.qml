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
}
