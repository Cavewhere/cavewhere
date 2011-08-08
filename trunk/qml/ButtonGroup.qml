import QtQuick 1.0

Item {
    property alias text: groupText.text
    default property alias buttons: buttonArea.children

    width: childrenRect.width
    height: childrenRect.height

    Rectangle {
        id: buttonAreaRect

        width: buttonArea.width + 5
        height: buttonArea.height + textRect.height / 2.0

        border.width: 1
        border.color: "black"

        //color: "gray"

        radius: 3

        Row {
            id: buttonArea
            anchors.horizontalCenter: buttonAreaRect.horizontalCenter

            y: 2

            width: childrenRect.width
            height: childrenRect.height

            spacing: 3
        }
    }

    Rectangle {
        id: textRect

        anchors.verticalCenter: buttonAreaRect.bottom
        anchors.horizontalCenter: buttonAreaRect.horizontalCenter

        width: groupText.width + 4
        height: groupText.height + 3

        radius: 3
//        color: "white"
        border.width: 1

        Text {
            id: groupText

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1

            font.pointSize: 9
            font.bold: true
        }
    }

}
