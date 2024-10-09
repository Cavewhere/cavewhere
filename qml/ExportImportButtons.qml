/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import Cavewhere 1.0
import QtQuick.Controls as Controls;
import QtQuick.Dialogs

QQ.Item {
    id: iconBar

    property alias currentTrip: exportManager.trip
    property alias currentCave: exportManager.cave
    property alias currentRegion: exportManager.cavingRegion
    property alias importVisible: importButton.visible
    property Page page //The page that the ExportImportButtons exist on

    implicitWidth: rowId.width
    implicitHeight: rowId.height


    SurveyExportManager {
        id: exportManager
    }

    QQ.Item {
        id: fileDialogItem
        FileDialog {
            id: fileDialog
            currentFolder: rootData.lastDirectory
        }

        states: [
            QQ.State {
                name: "EXPORT_TRIP_SURVEX"
                QQ.PropertyChanges {
                    target: fileDialog
                    title: "Export " + exportManager.currentTripName + " to survex"
                    nameFilters: ["Survex (*.svx)"]
                    selectExisting: false
                    selectMultiple: false
                    onAccepted: {
                        rootData.lastDirectory = fileUrl
                        exportManager.exportSurvexTrip(fileUrl);
                    }
                }
            },
            QQ.State {
                name: "EXPORT_CAVE_SURVEX"
                QQ.PropertyChanges {
                    target: fileDialog
                    title: "Export " + exportManager.currentCaveName + " to survex"
                    nameFilters: ["Survex (*.svx)"]
                    selectExisting: false
                    selectMultiple: false
                    onAccepted: {
                        rootData.lastDirectory = fileUrl
                        exportManager.exportSurvexCave(fileUrl);
                    }
                }
            },
            QQ.State {
                name: "EXPORT_REGION_SURVEX"
                QQ.PropertyChanges {
                    target: fileDialog
                    title: "Export All Caves to survex"
                    nameFilters: ["Survex (*.svx)"]
                    selectExisting: false
                    selectMultiple: false
                    onAccepted: {
                        rootData.lastDirectory = fileUrl
                        exportManager.exportSurvexRegion(fileUrl);
                    }
                }
            },
            QQ.State {
                name: "EXPORT_CAVE_COMPASS"
                QQ.PropertyChanges {
                    target: fileDialog
                    title: "Export " + exportManager.currentCaveName + " to compass"
                    nameFilters: ["Compass (*.dat)"]
                    selectExisting: false
                    selectMultiple: false
                    onAccepted: {
                        rootData.lastDirectory = fileUrl
                        exportManager.exportCaveToCompass(fileUrl);
                    }
                }
            },
            QQ.State {
                name: "EXPORT_CAVE_CHIPDATA"
                QQ.PropertyChanges {
                    target: fileDialog
                    title: "Export " + exportManager.currentCaveName + " to chipdata"
                    nameFilters: ["Chipdata (*.*)"]
                    selectExisting: false
                    selectMultiple: false
                    onAccepted: {
                        exportManager.exportCaveToChipdata(fileUrl);
                    }
                }
            },
            QQ.State {
                name: "IMPORT_COMPASS"
                QQ.PropertyChanges {
                    target: fileDialog
                    title: "Import from Compass"
                    nameFilters: ["Compass (*.dat)"]
                    selectExisting: true
                    selectMultiple: true
                    onAccepted: {
                        rootData.lastDirectory = fileUrl
                        rootData.surveyImportManager.importCompassDataFile(fileUrls);
                    }
                }
            }
        ]
    }

    QQ.Row {
        id: rowId

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
                        currentText: exportManager.currentTripName
                        onTriggered: {
                            fileDialogItem.state = "EXPORT_TRIP_SURVEX"
                            fileDialog.open()
                        }
                    }

                    ExportSurveyMenuItem {
                        prefixText: "Current cave"
                        currentText: exportManager.currentCaveName
                        onTriggered: {
                            fileDialogItem.state = "EXPORT_CAVE_SURVEX"
                            fileDialog.open()
                        }
                    }

                    Controls.MenuItem {
                        text: "Region (all caves)"
                        onTriggered: {
                            fileDialogItem.state = "EXPORT_REGION_SURVEX"
                            fileDialog.open()
                        }
                    }
                }

                Controls.Menu {
                    title: "Compass"

                    ExportSurveyMenuItem {
                        prefixText: "Current cave"
                        currentText: exportManager.currentCaveName
                        onTriggered: {
                            fileDialogItem.state = "EXPORT_CAVE_COMPASS"
                            fileDialog.open()
                        }
                    }
                }

                Controls.Menu {
                    title: "Chipdata"

                    ExportSurveyMenuItem {
                        prefixText: "Current cave"
                        currentText: exportManager.currentCaveName
                        onTriggered: {
                            fileDialogItem.state = "EXPORT_CAVE_CHIPDATA"
                            fileDialog.open()
                        }
                    }
                }
            }
        }

        Button {
            id: importButton

            text: "Import"
            visible: false

            onClicked: {
                importContextMenu.popup()
            }

            Controls.Menu {
                id: importContextMenu

                Controls.MenuItem {
                    text: "Survex (.svx)"
                    onTriggered: rootData.surveyImportManager.importSurvex()
                }

                Controls.MenuItem {
                    text: "Compass (.dat)"
                    onTriggered: {
                        fileDialogItem.state = "IMPORT_COMPASS"
                        fileDialog.open()
                    }
                }

                Controls.MenuItem {
                    text: "Walls (.wpj)"
                    onTriggered: rootData.surveyImportManager.importWalls()
                }

                Controls.MenuItem {
                    text: "Walls (.srv)"
                    onTriggered: rootData.surveyImportManager.importWallsSrv()
                }

                Controls.MenuItem {
                    text: "CSV (.csv)"
                    onTriggered: {
                        var oldImportExport = iconBar.page.childPage("CSV Import");
                        if(oldImportExport !== rootData.pageSelectionModel.currentPage) {
                            if(oldImportExport !== null) {
                                rootData.pageSelectionModel.unregisterPage(oldImportExport);
                            }

                            var page = rootData.pageSelectionModel.registerPage(iconBar.page,
                                                                                "CSV Importer",
                                                                                csvImporterComponent,
                                                                                {});
                            rootData.pageSelectionModel.gotoPage(page);
                        }
                    }
                }
            }
        }
    }

    QQ.Component {
        id: csvImporterComponent
        CSVImporterPage {

        }
    }
}
