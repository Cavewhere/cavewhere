import QtQuick as QQ
import QtQuick.Controls

QQ.Rectangle {
    default property alias scollData: itemId.data

    anchors.fill: parent

    ScrollView {
        anchors.fill: parent
        clip: true

        // This need to be defined so we can access it in qml test
        ScrollBar.vertical: ScrollBar {
            objectName: "verticalScrollBar"
        }

        QQ.Item {
            id: itemId
            x: 10
            y: x
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
