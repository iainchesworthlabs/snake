// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// A question that has to be answered before anything else is touched: a dimmed window with a dialog on it that has a
// message and two buttons. The keyboard starts on the safe answer, `cancelText`, and Escape gives it. The owner shows
// and hides the dialog with `visible`.
FocusScope {
    id: root

    property string title
    property string message
    property string confirmText
    property string cancelText

    signal confirmed
    signal cancelled

    onVisibleChanged: {
        if (visible)
            cancelButton.forceActiveFocus();
    }

    Accessible.role: Accessible.Dialog
    Accessible.name: title

    Keys.onEscapePressed: root.cancelled()

    // The dimming covers the whole window and takes the clicks, so nothing below it is touched.
    Rectangle {
        anchors.fill: parent
        color: Theme.overlay

        MouseArea {
            anchors.fill: parent
        }
    }

    PhonePage {
        Item {
            anchors.centerIn: parent
            width: 330
            height: dialogColumn.height + 44

            Rectangle {
                y: 8
                width: parent.width
                height: parent.height
                radius: 20
                color: Theme.shadow
            }

            Rectangle {
                width: parent.width
                height: parent.height
                radius: 20
                color: Theme.surface
                border.width: 4
                border.color: Theme.ink
            }

            Column {
                id: dialogColumn

                x: 22
                y: 22
                width: parent.width - 44
                spacing: 14

                Text {
                    objectName: "confirmTitle"
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: Theme.ink
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    text: root.title
                }

                Text {
                    objectName: "confirmMessage"
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: Theme.ink
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    text: root.message
                }

                ToyButton {
                    id: cancelButton

                    objectName: "cancelButton"
                    width: parent.width
                    height: 52
                    focus: true
                    kind: "primary"
                    fontSize: 22
                    drop: 5
                    text: root.cancelText
                    onClicked: root.cancelled()
                }

                ToyButton {
                    objectName: "confirmButton"
                    width: parent.width
                    height: 46
                    kind: "accent"
                    text: root.confirmText
                    onClicked: root.confirmed()
                }
            }
        }
    }
}
