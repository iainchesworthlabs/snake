// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The text of a licence or of the notices, which scrolls: by the finger or the wheel, and with the arrow keys and Page
// Up and Page Down.
FocusScope {
    id: root

    property string title
    property string body

    // The player left the screen: About shows next.
    signal closed

    onVisibleChanged: {
        if (visible) {
            scroller.contentY = 0;
            forceActiveFocus();
        }
    }

    Accessible.role: Accessible.Pane
    Accessible.name: title

    function scrollBy(distance) {
        scroller.contentY = Math.max(0, Math.min(scroller.contentHeight - scroller.height, scroller.contentY + distance));
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Escape) {
            root.closed();
        } else if (event.key === Qt.Key_Down) {
            root.scrollBy(48);
        } else if (event.key === Qt.Key_Up) {
            root.scrollBy(-48);
        } else if (event.key === Qt.Key_PageDown) {
            root.scrollBy(scroller.height - 48);
        } else if (event.key === Qt.Key_PageUp) {
            root.scrollBy(48 - scroller.height);
        } else {
            return;
        }
        event.accepted = true;
    }

    PhonePage {
        id: page

        landscapeLayout: true

        ToyButton {
            objectName: "noticeBackButton"
            x: Theme.mirrored(15, width, page.pageWidth)
            y: page.landscape ? 10 : 28
            width: page.landscape ? 42 : 46
            height: page.landscape ? 42 : 46
            radius: 10
            activeFocusOnTab: false
            text: LayoutMirroring.enabled ? ">" : "<"
            fontSize: 24
            Accessible.name: qsTr("Back")
            onClicked: root.closed()
        }

        ShadowText {
            objectName: "noticeTitle"
            x: Theme.mirrored(page.landscape ? 66 : 74, width, page.pageWidth)
            maxWidth: page.landscape ? page.pageWidth - 66 - 15 : 300
            y: page.landscape ? 12 : 26
            offset: 3
            pixelSize: 28
            text: root.title
        }

        Rectangle {
            x: 15
            y: page.landscape ? 60 : 96
            width: page.pageWidth - 30
            height: page.landscape ? 288 : 732
            radius: 14
            color: Theme.surface
            border.width: 4
            border.color: Theme.ink

            Flickable {
                id: scroller

                objectName: "noticeScroller"
                x: 12
                y: 12
                width: parent.width - 24
                height: parent.height - 24
                clip: true
                // The licences are in English: they read from the left whatever the language of the game is.
                LayoutMirroring.enabled: false
                LayoutMirroring.childrenInherit: true
                contentWidth: width
                contentHeight: bodyText.height
                boundsBehavior: Flickable.StopAtBounds

                Text {
                    id: bodyText

                    objectName: "noticeBody"
                    width: scroller.width
                    wrapMode: Text.Wrap
                    color: Theme.ink
                    font.pixelSize: 12
                    text: root.body
                }
            }
        }
    }
}
