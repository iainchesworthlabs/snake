// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// About: the name and version, the licence of the program, where the source is and which revision this is, and a
// button for the text of each licence (the program's, the third-party components' and the font's).
FocusScope {
    id: root

    required property ProfileViewModel profile
    // How many times the version has been tapped; five show the details for a bug report.
    property int versionTaps: 0

    // The player left the screen: the settings show next.
    signal closed
    // The player asked to read a licence: "licence", "notices" or "font".
    signal noticeRequested(string kind)

    onVisibleChanged: {
        if (visible)
            licenceButton.forceActiveFocus();
    }

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("About")

    Keys.onEscapePressed: root.closed()

    PhonePage {
        id: page

        landscapeLayout: true

        // Lying down the facts are on the leading side and the buttons on the other.
        readonly property real columnWidth: (pageWidth - 3 * 15) / 2
        readonly property real leadingX: Theme.mirrored(15, columnWidth, pageWidth)
        readonly property real trailingX: Theme.mirrored(2 * 15 + columnWidth, columnWidth, pageWidth)

        ToyButton {
            objectName: "aboutBackButton"
            x: Theme.mirrored(15, width, page.pageWidth)
            y: page.landscape ? 10 : 28
            width: page.landscape ? 42 : 46
            height: page.landscape ? 42 : 46
            radius: 10
            text: LayoutMirroring.enabled ? ">" : "<"
            fontSize: 24
            Accessible.name: qsTr("Back")
            onClicked: root.closed()
        }

        ShadowText {
            x: Theme.mirrored(page.landscape ? 66 : 74, width, page.pageWidth)
            maxWidth: 300
            y: page.landscape ? 8 : 26
            offset: 3
            pixelSize: page.landscape ? 30 : 34
            text: qsTr("About")
        }

        Item {
            x: page.landscape ? page.leadingX : 15
            y: page.landscape ? 62 : 100
            width: page.landscape ? page.columnWidth : 360
            height: panel.height + 6

            Rectangle {
                y: 6
                width: parent.width
                height: panel.height
                radius: 18
                color: Theme.shadow
            }

            Rectangle {
                id: panel

                width: parent.width
                height: panelColumn.height + 36
                radius: 18
                color: Theme.surface
                border.width: 4
                border.color: Theme.ink

                Column {
                    id: panelColumn

                    x: 18
                    y: 18
                    width: parent.width - 36
                    spacing: 8

                    Text {
                        color: Theme.ink
                        font.pixelSize: 34
                        font.weight: Font.Bold
                        text: qsTr("Snake")
                    }

                    Text {
                        objectName: "aboutVersion"
                        color: Theme.ink
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        text: qsTr("Version %1").arg(AboutInfo.version)

                        // Five taps show the details a bug report needs (the language chosen, the save file): they are for
                        // whoever is asked for them, and not for the page.
                        TapHandler {
                            onTapped: root.versionTaps += 1
                        }
                    }

                    Text {
                        objectName: "aboutLicence"
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Theme.ink
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        text: qsTr("Free software, licensed under the %1.").arg(AboutInfo.licence)
                    }

                    Text {
                        objectName: "aboutSource"
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        color: Theme.ink
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        text: qsTr("Source: %1").arg(AboutInfo.repositoryUrl)
                    }

                    Text {
                        objectName: "aboutRevision"
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        color: Theme.ink
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        text: qsTr("Revision: %1").arg(AboutInfo.shortRevision)
                    }

                    // Not translated: for a bug report about the language.
                    Text {
                        objectName: "aboutLanguage"
                        visible: root.versionTaps >= 5
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        color: Theme.ink
                        font.pixelSize: 12
                        text: "Language: " + AboutInfo.languageDetail
                    }

                    Text {
                        objectName: "aboutSaveStatus"
                        visible: root.versionTaps >= 5
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        color: Theme.ink
                        font.pixelSize: 12
                        text: "Saves: " + root.profile.saveStatus
                    }
                }
            }
        }

        Column {
            x: page.landscape ? page.trailingX : 15
            y: page.landscape ? 62 : 100 + panel.height + 30
            width: page.landscape ? page.columnWidth : 360
            spacing: page.landscape ? 10 : 12

            ToyButton {
                id: licenceButton

                objectName: "licenceButton"
                width: parent.width
                height: page.landscape ? 44 : 50
                focus: true
                text: qsTr("Licence")
                onClicked: root.noticeRequested("licence")
            }

            ToyButton {
                objectName: "noticesButton"
                width: parent.width
                height: page.landscape ? 44 : 50
                text: qsTr("Third-party notices")
                onClicked: root.noticeRequested("notices")
            }

            ToyButton {
                objectName: "fontLicenceButton"
                width: parent.width
                height: page.landscape ? 44 : 50
                text: qsTr("Font licence")
                onClicked: root.noticeRequested("font")
            }

            ToyButton {
                objectName: "sourceButton"
                width: parent.width
                height: page.landscape ? 44 : 50
                text: qsTr("Open the source page")
                onClicked: Qt.openUrlExternally(AboutInfo.repositoryUrl)
            }
        }
    }
}
