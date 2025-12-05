import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
QQ.Row {
    spacing: 0

    QQ.Image {
        id: imageId
        source: "qrc:/icons/cave512x512.png"
        width: 96
        height: 96
    }

    QQ.Item {
       width: 10
       height: 1
    }

    Text {
        id: caveTextId
        QQ.FontLoader { id: fontBold; source: "qrc:/fonts/YanoneKaffeesatz-Bold.ttf"        }

        text: "CAVE"
        font.pixelSize: 40
        anchors.verticalCenter: imageId.verticalCenter
        font.family: fontBold.name
        font.letterSpacing: 3

    }

    Text {
        QQ.FontLoader { id: fontThin; source: "qrc:/fonts/YanoneKaffeesatz-Thin.ttf" }

        text: "WHERE"
        font.pixelSize: caveTextId.font.pixelSize
        anchors.verticalCenter: imageId.verticalCenter
        font.family: fontThin.name
    }
}
