// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

Rectangle {
    id: fileMenuButton

    width: 100
    height: Math.max(caveIcon.height, textItem.height)

    anchors.top: parent.top
    anchors.left: parent.left

    Image {
        id: caveIcon
        source: "qrc:/icons/cave.png"
        anchors.left: parent.left
    }

    Text {
        id: textItem
        anchors.left: caveIcon.right
        anchors.leftMargin: 3

        text: "Cavewhere"
    }

    MouseArea {
        anchors.fill: parent
        onClicked: fileMenu.popupOnTopOf(fileMenuButton, 0, fileMenuButton.height)
    }

    ContextMenu {
        id: fileMenu

        MenuItem {
            text: "New"
            onTriggered: project.newProject();
        }

        MenuItem {
            text: "Save"
        }

        MenuItem {
            text: "Save As"
        }

        MenuItem {
            text: "Load"
        }

        MenuItem {
            text: "Compute Scraps"
        }

        MenuItem {
            text: "Quit"
        }
    }
}
