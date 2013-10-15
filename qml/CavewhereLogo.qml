import QtQuick 2.0

Row {
    spacing: 10

    Image {
        id: imageId
        source: "qrc:/icons/cave-64x64.png"
    }

    Text {
        text: "Cavewhere"
        font.pointSize: 40
        anchors.verticalCenter: imageId.verticalCenter
    }
}
