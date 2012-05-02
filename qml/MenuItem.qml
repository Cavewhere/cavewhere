// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

Item {
    id: menuItemId

    property alias text: textId.text
    property alias containsMouse: mouseArea.containsMouse
    property bool selected: false
    property var parentContextMenu: parent.parent

    width: textId.width
    height: textId.height


    Rectangle {
        id: selectionBackground
        color: "blue"
        visible: selected
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parentContextMenu.width
        x: -parentContextMenu.marginWidth
    }

    Text {
        id: textId
        color: selected ? "white" : "black"
    }

    MouseArea {
        id: mouseArea

        anchors.fill: selectionBackground
        hoverEnabled: true

        onContainsMouseChanged: {
            if(containsMouse) {
                //ContextMenu parent
                menuItemId.parent.parent.setCurrentItem(menuItemId)
            }
        }
    }
}

