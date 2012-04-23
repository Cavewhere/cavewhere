import QtQuick 2.0

Rectangle {
    id: container
    property alias iconSource: iconNormal.source
    property alias hoverIconSource: iconHover.source
    property alias sourceSize: iconNormal.sourceSize;
    property alias text: label.text
    property bool selected: false

    signal clicked();

    radius: 3

    height: iconNormal.sourceSize.height + label.height
    width:  Math.max(iconNormal.sourceSize.width, label.width) + 4

    color: selected ? "#999999" : "#00000000"


    Image {
        id: iconNormal
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        sourceSize: container.sourceSize
        visible: true;
    }

    Image {
        id: iconHover
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        sourceSize: iconNormal.sourceSize
        visible: false;
    }

    Text {
        id: label
//        anchors.bottom: parent.bottom
        anchors.top: iconNormal.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 2
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent

        hoverEnabled: true

        onClicked: {
            container.clicked();
        }
    }

    Behavior on color {
        PropertyAnimation { }
    }

    states: [
        State {
            name: "hover"; when: mouseArea.containsMouse && iconHover.status == Image.Ready
            PropertyChanges { target: iconHover; visible: true }
            PropertyChanges { target: iconNormal; visible: false }
            //            PropertyChanges { target: buttonText; font.bold: true }
        }

    ]

}
