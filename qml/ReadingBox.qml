import QtQuick 2.0

DataBox {
    id: readBox;

    property alias readingText: readingTextObj.text

    Text {
        id: readingTextObj
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: 2
        anchors.topMargin: 1
        font.pixelSize: 10
    }
}
