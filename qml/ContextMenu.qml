// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0

Rectangle {
    id: menuWindow

    default property alias children: childItems.children
    property var currentItem: null
    property int marginWidth: 15
    property int marginHeight: 3

    border.width: 1
    radius: 3
    color: Qt.rgba(1.0, 1.0, 1.0, 0.9)

    width: childItems.width + marginWidth * 2
    height: childItems.height + marginHeight * 2
    parent: globalMenuMouseHandler
    visible: false

    function setCurrentItem(item) {
        if(currentItem !== null) {
            currentItem.selected = false
        }

        currentItem = item

        if(currentItem !== null) {
            currentItem.selected = true
        }
    }

    function addToGlobalMenuMouseHandler() {
        if(globalMenuMouseHandler.currentRootContextMenu === null) {
            globalMenuMouseHandler.currentRootContextMenu = menuWindow
        }
    }

    onVisibleChanged: {
        if(visible) {
            addToGlobalMenuMouseHandler();
        }
    }

    Column {
        id: childItems
        x: marginWidth
        y: marginHeight
    }

}
