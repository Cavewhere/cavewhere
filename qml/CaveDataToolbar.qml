/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0
import QtQuick.Controls 1.2 as Controls;

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
                exportContextMenu.popup();
            }

            Controls.Menu {
                id: exportContextMenu

                Controls.Menu {
                    title: "Survex"

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

                    Controls.MenuItem {
                        text: "Region (all caves)"
                        onTriggered: surveyExportManager.openExportSurvexRegionFileDialog()
                    }
                }

                Controls.Menu {
                    title: "Compass"

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
                importContextMenu.popup()
            }

            Controls.Menu {
                id: importContextMenu

                Controls.MenuItem {
                    text: "Survex (.svx)"
                    onTriggered: surveyImportManager.importSurvex()
                }

                Controls.MenuItem {
                    text: "Compass (.dat)"
                    onTriggered: surveyImportManager.importCompassDataFile();
                }
            }
        }
    }
}
