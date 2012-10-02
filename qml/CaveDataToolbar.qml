// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
//import QtDesktop 0.2 as Desktop
import Cavewhere 1.0

Item {
    id: iconBar


    Row {

        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.verticalCenter: parent.verticalCenter

        spacing: 3

        Button {
            id: exportButton
            text: "Export"

            onClicked: {
                exportContextMenu.popupOnTopOf(exportButton, 0, exportButton.height)
            }

            ContextMenu {
                id: exportContextMenu

                Menu {
                    text: "Survex"

                    ExportSurveyMenuItem {
                        prefixText: "Current trip"
                        currentText: surveyExportManager.currentTripName
                        onTriggered: surveyExportManager.openExportSurvexTripFileDialog()
                    }

                    ExportSurveyMenuItem {
                        prefixText: "Current cave"
                        currentText: surveyExportManager.currentCaveName
                        onTriggered: surveyExportManager.openExportSurvexCaveFileDialog()
                    }

                    MenuItem {
                        text: "Region (all caves)"
                        onTriggered: surveyExportManager.openExportSurvexRegionFileDialog()
                    }
                }

                Menu {
                    text: "Compass"

                    ExportSurveyMenuItem {
                        prefixText: "Current cave"
                        currentText: surveyExportManager.currentCaveName
                        onTriggered: surveyExportManager.openExportCompassCaveFileDialog()
                    }
                }
            }
        }

        Button {
            id: importButton

            text: "Import"

            onClicked: {
                importContextMenu.popupOnTopOf(importButton, 0, importButton.height)
            }

            ContextMenu {
                id: importContextMenu

                MenuItem {
                    text: "Survex (.svx)"
                    onTriggered: surveyImportManager.importSurvex()
                }

                MenuItem {
                    text: "Compass (.dat)"
                    onTriggered: surveyImportManager.importCompassDataFile();
                }
            }
        }
    }
}
