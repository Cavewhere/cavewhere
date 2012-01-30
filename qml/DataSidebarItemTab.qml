import QtQuick 1.1

Rectangle {
    property bool selected: false;

    width:  parent.width
    x: selected ? parent.x - parent.anchors.leftMargin : parent.x + parent.width
    color: "#00000000"

    anchors.top: parent.top
    anchors.bottom: parent.bottom

    StandardBorder {
        anchors.rightMargin: -7
    }

    Behavior on x {
        NumberAnimation {
            duration: 200;
            easing.type: Easing.OutQuad
        }
    }
}
