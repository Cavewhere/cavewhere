import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
QQ.Rectangle {
    default property alias scollData: itemId.data

    anchors.fill: parent
    color: Theme.background


    QC.ScrollView {
        anchors.fill: parent
        clip: true

        // This need to be defined so we can access it in qml test
        QC.ScrollBar.vertical: QC.ScrollBar {
            objectName: "verticalScrollBar"
        }

        QQ.Item {
            id: itemId
            x: 10
            y: 10
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
