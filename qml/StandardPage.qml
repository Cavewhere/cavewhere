import QtQuick 2.0 as QQ

QQ.Rectangle {
    default property alias pageItemData: pageItemId.data

    anchors.fill: parent

    QQ.Item {
        id: pageItemId
        anchors.fill: parent
        anchors.margins: 10
    }

}
