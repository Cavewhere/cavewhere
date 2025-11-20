import QtQuick
import QtQuick.Controls

Item {

    property alias button: buttonId

    implicitWidth: buttonId.implicitWidth
    implicitHeight: buttonId.implicitHeight

    function shake() {
        shakeAnim.restart()
    }

    Button {
        id: buttonId
    }

    // A quick left-right wiggle
    SequentialAnimation {
        id: shakeAnim
        loops: 3               // shake back-and-forth 3 times
        running: false
        // move left
        NumberAnimation {
            target: buttonId
            property: "x"
            from: 0
            to:   -8
            duration: 50
            easing.type: Easing.InOutQuad
        }
        // move right
        NumberAnimation {
            target: buttonId
            property: "x"
            from: -8
            to:   8
            duration: 100
            easing.type: Easing.InOutQuad
        }
        // back to center
        NumberAnimation {
            target: buttonId
            property: "x"
            from: 8
            to:   0
            duration: 50
            easing.type: Easing.InOutQuad
        }
    }
}
