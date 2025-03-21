pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC

QQ.ListView {
    id: listId

    currentIndex: 0
    delegate: QC.TabButton {
        required property string modelData
        required property int index

        text: modelData
        anchors.left: parent.left
        anchors.right: parent.right
        onClicked: {
            listId.currentIndex = index
        }
        checked: listId.currentIndex == index
    }
}
