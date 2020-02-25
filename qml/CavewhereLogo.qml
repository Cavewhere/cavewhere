import QtQuick 2.0 as QQ

QQ.Row {
    spacing: 10

    QQ.Image {
        id: imageId
        source: "qrc:/icons/cave-64x64.png"
    }

    Text {
        text: "CaveWhere"
        font.pointSize: 40
        anchors.verticalCenter: imageId.verticalCenter
    }
}
