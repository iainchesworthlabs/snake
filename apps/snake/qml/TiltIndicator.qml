// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The tilt of the device, as a marble on a plate: the plate is the range of tilt that is drawn, the inner square the
// dead zone where the snake is left alone, and the outer one the angle that turns it. The dot is where the device is
// tipped now, and it is coral while the tilt steers. The player holds the device and sees at once why it does not turn.
Item {
    id: root

    required property InputController input

    // The degrees from the middle to the edge of the plate.
    readonly property real range: 30
    readonly property real half: width / 2

    implicitWidth: 132
    implicitHeight: 132

    Accessible.role: Accessible.Indicator
    Accessible.name: qsTr("Tilt")

    // Three taps show what the sensor says, for a bug report; three more hide it.
    property int taps: 0
    readonly property bool readout: taps % 6 >= 3

    Rectangle {
        y: 4
        width: parent.width
        height: parent.height
        radius: 18
        color: Theme.shadow
    }

    Rectangle {
        anchors.fill: parent
        radius: 18
        color: Theme.surface
        border.width: 4
        border.color: Theme.ink
    }

    // The angle that turns the snake, and the dead zone inside it.
    Rectangle {
        objectName: "tiltThreshold"
        anchors.centerIn: parent
        width: 2 * root.input.threshold / root.range * root.half
        height: width
        radius: 8
        color: "transparent"
        border.width: 2
        border.color: Theme.ink
        opacity: 0.4
    }

    Rectangle {
        objectName: "tiltDeadZone"
        anchors.centerIn: parent
        width: 2 * root.input.deadZone / root.range * root.half
        height: width
        radius: 6
        color: "transparent"
        border.width: 2
        border.color: Theme.ink
    }

    // Right is positive to the right; up is positive up, which is the other way on the screen.
    Rectangle {
        objectName: "tiltDot"
        readonly property real shownRight: Math.max(-root.range, Math.min(root.range, root.input.tiltRight))
        readonly property real shownUp: Math.max(-root.range, Math.min(root.range, root.input.tiltUp))

        x: root.half + shownRight / root.range * (root.half - 14) - width / 2
        y: root.half - shownUp / root.range * (root.half - 14) - height / 2
        width: 20
        height: 20
        radius: 6
        color: root.input.tiltHeading !== InputController.Heading.None ? Theme.accent : Theme.ink
        border.width: 3
        border.color: Theme.ink
    }

    TapHandler {
        onTapped: root.taps += 1
    }

    // Not translated. The sensor as it came (g; flat and face up reads z of 1), then the angles from where it was calibrated.
    Text {
        objectName: "tiltReadout"
        visible: root.readout
        anchors.top: parent.bottom
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        horizontalAlignment: Text.AlignHCenter
        color: Theme.ink
        font.pixelSize: 12
        text: "x " + root.input.rawX.toFixed(2) + "  y " + root.input.rawY.toFixed(2) + "  z " + root.input.rawZ.toFixed(2) + "  n " + root.input.rawCount + "\n" + "right " + root.input.tiltRight.toFixed(1) + "\u00B0  up " + root.input.tiltUp.toFixed(1) + "\u00B0"
    }
}
