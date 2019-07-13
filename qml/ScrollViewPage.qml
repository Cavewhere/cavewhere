import QtQuick 2.0
import QtQuick.Controls 2.5

Rectangle {
    default property alias scollData: itemId.data

    anchors.fill: parent

    ScrollView {
        anchors.fill: parent
        clip: true

        Item {
            id: itemId
            x: 10
            y: x
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
