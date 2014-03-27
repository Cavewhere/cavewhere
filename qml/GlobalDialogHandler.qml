import QtQuick 2.0

Item {
    anchors.fill: parent

    MouseArea {
        anchors.fill: parent
        onPressed: mouse.accepted = true
        onWheel: wheel.accepted = true
        enabled: false
    }
}
