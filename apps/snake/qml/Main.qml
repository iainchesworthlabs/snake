// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Window {
    required property int windowWidth
    required property int windowHeight

    width: windowWidth
    height: windowHeight
    visible: true
    title: qsTr("Snake")
    color: "#101820"

    GameViewModel {
        id: game
    }

    Component.onCompleted: game.start()

    Text {
        anchors.centerIn: parent
        color: "#e0e6ea"
        font.pixelSize: 24
        text: qsTr("Frame %1").arg(game.frame)
    }
}
