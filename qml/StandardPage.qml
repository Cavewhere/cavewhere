import QtQuick 2.0

Rectangle {
    default property alias pageItemData: pageItemId.data

    anchors.fill: parent

    Item {
        id: pageItemId
        anchors.fill: parent
        anchors.margins: 10
    }

}
