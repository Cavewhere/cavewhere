// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
import QtDesktop 0.1 as Desktop
import Cavewhere 1.0

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
            id: importButton

            text: "Import"


            onClicked: {
                var globalPoint = mapToItem(null, 0, 0);
                importContextMenu.showPopup(globalPoint.x, globalPoint.y + 2 * importButton.height)
            }

            Desktop.ContextMenu {
                id: importContextMenu

                Desktop.MenuItem {
                    text: "Survex (.svx)"

                    onTriggered: {
                        //Open the survex importer
                        surveyImportManager.importSurvex()
                    }
                }

            }
        }
    }


    Desktop.ContextMenu {
        id: exportContextMenu
    }
}
