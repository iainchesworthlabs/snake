// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// A labelled switch of the Toybox Pixel style: the label on the left and, on the right, a track that is coral with the
// knob at the right end when on and cream with the knob at the left end when off. The owner changes the setting when
// it gets `clicked`, so the switch always shows what the setting is. The keyboard reaches it with Tab or the up and
// down arrows, and Enter or Space press it. The focus shows as a ring.
Item {
    id: root

    property string text
    property bool checked
    property int fontSize: 20

    signal clicked

    // A press, by touch, mouse or key: the program hears it (for the button sound) and then the owner gets `clicked`.
    function activate() {
        UiEvents.buttonPressed();
        clicked();
    }

    implicitHeight: 44

    activeFocusOnTab: true
    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checked: checked
    Accessible.onToggleAction: root.activate()
    Accessible.onPressAction: root.activate()

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            if (!event.isAutoRepeat)
                root.activate();
            event.accepted = true;
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            root.nextItemInFocusChain(event.key === Qt.Key_Down).forceActiveFocus(Qt.TabFocusReason);
            event.accepted = true;
        }
    }

    Text {
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.ink
        font.pixelSize: root.fontSize
        font.weight: Font.DemiBold
        text: root.text
    }

    Rectangle {
        id: track

        objectName: "track"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 60
        height: 34
        radius: 10
        color: root.checked ? Theme.accent : Theme.surface
        border.width: 3
        border.color: Theme.ink

        Rectangle {
            x: root.checked ? track.width - width - 7 : 7
            anchors.verticalCenter: parent.verticalCenter
            width: 22
            height: 22
            radius: 6
            color: Theme.ink
        }
    }

    Rectangle {
        visible: root.activeFocus
        x: -6
        y: -4
        width: root.width + 12
        height: root.height + 8
        radius: 14
        color: "transparent"
        border.width: 3
        border.color: Theme.accent
    }

    TapHandler {
        onTapped: root.activate()
    }
}
