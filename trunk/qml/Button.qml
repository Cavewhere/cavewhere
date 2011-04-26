import QtQuick 1.0

Rectangle {
    id: button
    property alias text:  buttonText.text
    property alias iconSource: icon.source
    property bool troggled: false

    width: buttonText.width + icon.width + (2 * container.anchors.margins) + icon.anchors.rightMargin
    height: buttonText.height + (2 * container.anchors.margins)

    radius: 3

    border.width: 1
    border.color: "black"

    gradient: Gradient {
        GradientStop {id: stop1; position:0.0; color:"#B7BDC5" }
        GradientStop {id: stop2; position:1.0; color:"#7F838C" }
    }

    Item {
        id: container

        anchors.fill: parent
        anchors.margins: 4

        Image {
            id: icon

            height: status == Image.Ready > 0 ? 16 : 0
            width: status == Image.Ready > 0 ? 16 : 0

            anchors.right: buttonText.left
            anchors.rightMargin: status == Image.Ready > 0 ? 3 : 0
            //anchors.left: parent.left

            visible: status == Image.Ready
        }

        Text {
            id: buttonText
            anchors.right: parent.right

        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent

        hoverEnabled: true
    }

    states: [
        State {
            name: "hover"; when: mouseArea.containsMouse
            PropertyChanges { target: stop1; color: "#B7BDC5" }
            PropertyChanges { target: stop2; color: "#29335B" }
            //            PropertyChanges { target: buttonText; font.bold: true }
        },

        State {
        name: "clicked"

}

    ]

}
