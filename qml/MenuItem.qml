// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

Item {
    id: menuItemId

    property alias text: textId.text
    property alias containsMouse: mouseArea.containsMouse
    property bool selected: false
    property var parentContextMenu: parent.parent
    property bool enabled: true
    property bool menu: false

    width: textId.width
    height: textId.height

    signal triggered;

    Rectangle {
        id: selectionBackground
        color: "blue"
        visible: selected && enabled
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parentContextMenu.width
        x: -parentContextMenu.marginWidth
    }

    Image {
        id: moreIcon
        visible: menu
        source: selectionBackground.visible ? "qrc:icons/moreArrowWhite.png" : "qrc:icons/moreArrow.png"
        anchors.right: selectionBackground.right
        anchors.verticalCenter: selectionBackground.verticalCenter
        anchors.rightMargin: 4
    }

    Text {
        id: textId
        color: menuItemId.enabled ? (selected ? "white" : "black") : "gray"
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

        onClicked: {
            if(enabled) {
                if(!menu) {
                    globalMenuMouseHandler.closeOpenMenu()
                }
                triggered();
            }
        }
    }
}

