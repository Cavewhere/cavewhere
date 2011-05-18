import Qt 4.7

Rectangle {
    property alias text: textArea.text

    width: textArea.width + 20
    height: 40

    border.color: "lightgray"
    border.width: 1;

    Text {
        id: textArea
        anchors.centerIn: parent
    }
}
