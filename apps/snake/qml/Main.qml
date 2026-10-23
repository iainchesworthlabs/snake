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
}
