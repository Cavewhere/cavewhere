import QtQuick as QQ

QQ.MouseArea {
    anchors.fill: parent
    onPressed: (mouse) => mouse.accepted = true
    onWheel: (wheel) => wheel.accepted = true
    enabled: false
    visible: enabled
}

