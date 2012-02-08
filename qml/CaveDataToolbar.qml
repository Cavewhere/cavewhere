// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
import QtDesktop 0.1 as Desktop

Item {
    id: iconBar


    Row {

        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.verticalCenter: parent.verticalCenter

        spacing: 3

        Button {
            text: "Export"

            onClicked: {
                exportContextMenu.
                exportContexMenu.showPopup(0, 0)
            }
        }


        Button {
            text: "Import"


        }
    }


    Desktop.ContextMenu {
        id: exportContextMenu
    }
}
