// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// A button of the Toybox Pixel style: a face on a hard drop shadow that the face sinks into when pressed. The kind
// sets the colours: "primary" is an ink face with cream text, "secondary" a cream face with an outline, "accent" the
// background colour with an outline, and "link" plain underlined text. The keyboard reaches it with Tab or the up and
// down arrows, and Enter or Space press it. The focus shows as a ring. A screen that takes its own keys sets
// activeFocusOnTab to false on its buttons, so they answer touch only.
Item {
    id: root

    property string text
    property string kind: "secondary"
    property int fontSize: 18
    property int drop: 4
    property int radius: 12

    signal clicked

    readonly property bool pressed: tap.pressed
    readonly property bool plain: kind === "link"

    // A button that is not enabled is faded and takes no touch or key.
    opacity: enabled ? 1 : 0.4
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: root.clicked()

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            if (!event.isAutoRepeat)
                root.clicked();
            event.accepted = true;
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            root.nextItemInFocusChain(event.key === Qt.Key_Down).forceActiveFocus(Qt.TabFocusReason);
            event.accepted = true;
        }
    }

    Rectangle {
        visible: !root.plain
        y: root.drop
        width: root.width
        height: root.height
        radius: root.radius
        color: Theme.shadow
    }

    Rectangle {
        id: face

        y: root.pressed && !root.plain ? root.drop - 1 : 0
        width: root.width
        height: root.height
        radius: root.radius
        color: root.plain ? "transparent" : root.kind === "primary" ? Theme.ink : root.kind === "accent" ? Theme.background : Theme.surface
        border.width: root.plain || root.kind === "primary" ? 0 : 3
        border.color: Theme.ink

        Text {
            anchors.centerIn: parent
            color: root.kind === "primary" ? Theme.surface : Theme.ink
            font.pixelSize: root.fontSize
            font.weight: root.plain ? Font.DemiBold : Font.Bold
            font.underline: root.plain
            text: root.text
        }
    }

    Rectangle {
        visible: root.activeFocus
        x: -6
        y: -6
        width: root.width + 12
        height: root.height + 12 + (root.plain ? 0 : root.drop)
        radius: root.radius + 6
        color: "transparent"
        border.width: 3
        border.color: Theme.accent
    }

    TapHandler {
        id: tap

        onTapped: root.clicked()
    }
}
