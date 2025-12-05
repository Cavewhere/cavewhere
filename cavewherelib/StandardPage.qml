import QtQuick as QQ

QQ.Item {
    default property alias pageItemData: pageItemId.data

    anchors.fill: parent

    QQ.Item {
        id: pageItemId
        anchors.fill: parent
        // anchors.margins: 10
    }

}
