import QtQuick 1.0

Rectangle {
    id: container
    property alias iconSource: iconNormal.source
    property alias hoverIconSource: iconHover.source
    property alias sourceSize: iconNormal.sourceSize;

    signal clicked();

    height: iconNormal.sourceSize.height
    width:  iconNormal.sourceSize.width

    Image {
        id: iconNormal
        anchors.centerIn: parent;
        sourceSize: container.sourceSize
        visible: true;
    }

    Image {
        id: iconHover
        anchors.centerIn: parent;
        sourceSize: iconNormal.sourceSize
        visible: false;
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent

        hoverEnabled: true

        onClicked: {
            container.clicked();
        }

    }

    states: [
        State {
            name: "hover"; when: mouseArea.containsMouse
            PropertyChanges { target: iconHover; visible: true }
            PropertyChanges { target: iconNormal; visible: false }
            //            PropertyChanges { target: buttonText; font.bold: true }
        }

    ]

}
