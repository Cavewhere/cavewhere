import QtQuick 2.0 as QQ
import QtQuick.Controls 2.12 as QC
import QtQuick.Layouts 1.12

RowLayout {
    id: rootId
    property bool supported: true
    property string text: ""

    QQ.Image {
        id: stopId
        source: "qrc:/icons/stopSignError.png"
        width: 16
        height: 16
        visible: !supported
    }

    QQ.Image {
        id: goodId
        source: "qrc:/icons/good.png"
        width: stopId.width
        height: stopId.height
        visible: supported
    }

    Text {
        id: textId

        text: {
            var supportText = supported ? "supported" : "unsupported"
            return rootId.text + " is " + supportText;
        }
    }
}
