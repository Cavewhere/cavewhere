import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib
RowLayout {
    id: rootId
    property bool supported: true
    property string text: ""

    QQ.Image {
        id: stopId
        source: "qrc:/icons/svg/stopSignError.svg"
        sourceSize: Qt.size(16, 16)
        visible: !rootId.supported
    }

    QQ.Image {
        id: goodId
        source: "qrc:/icons/good.png"
        sourceSize: stopId.sourceSize
        visible: rootId.supported
    }

    Text {
        id: textId

        text: {
            var supportText = rootId.supported ? "supported" : "unsupported"
            return rootId.text + " is " + supportText;
        }
    }
}
