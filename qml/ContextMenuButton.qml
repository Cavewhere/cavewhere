import QtQuick 2.0
import Cavewhere 1.0

Button {
    id: moreButton

    default property alias menuChildren: popupMenuId.children

    iconSource: "qrc:/icons/moreArrowDown.png"
    iconSize: Qt.size(8, 8)
    height: 13
    width: 13
    radius: 0

    onClicked: {
        popupMenuId.popupOnTopOf(moreButton, 0, moreButton.height)
    }

    ContextMenu {
        id: popupMenuId
    }

}
