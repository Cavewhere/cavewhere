import QtQuick as QQ
import QtQuick.Controls

QQ.Rectangle {
    default property alias scollData: itemId.data

    anchors.fill: parent

    ScrollView {
        anchors.fill: parent
        clip: true

        QQ.Item {
            id: itemId
            x: 10
            y: x
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
