import QtQuick 2.0

MouseArea {
    anchors.fill: parent
    onPressed: mouse.accepted = true
    onWheel: wheel.accepted = true
    enabled: false
    visible: enabled
}

