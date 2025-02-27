/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as Controls;
import QtQuick.Dialogs
import QtQuick.Layouts

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
            currentFolder: RootData.lastDirectory
        }

        states: [
            QQ.State {
                name: "EXPORT_TRIP_SURVEX"
                QQ.PropertyChanges {
                    fileDialog {
                        title: "Export " + exportManager.currentTripName + " to survex"
                        nameFilters: ["Survex (*.svx)"]
                        fileMode: FileDialog.SaveFile
                        onAccepted: {
                            RootData.lastDirectory = selectedFile
                            exportManager.exportSurvexTrip(selectedFile);
                        }
                    }
                }
            },
            QQ.State {
                name: "EXPORT_CAVE_SURVEX"
                QQ.PropertyChanges {
                    fileDialog {
                        title: "Export " + exportManager.currentCaveName + " to survex"
                        nameFilters: ["Survex (*.svx)"]
                        fileMode: FileDialog.SaveFile
                        onAccepted: {
                            RootData.lastDirectory = selectedFile
                            exportManager.exportSurvexCave(selectedFile);
                        }
                    }
                }
            },
            QQ.State {
                name: "EXPORT_REGION_SURVEX"
                QQ.PropertyChanges {
                    fileDialog {
                        title: "Export All Caves to survex"
                        nameFilters: ["Survex (*.svx)"]
                        fileMode: FileDialog.SaveFile
                        onAccepted: {
                            RootData.lastDirectory = selectedFile
                            exportManager.exportSurvexRegion(selectedFile);
                        }
                    }
                }
            },
            QQ.State {
                name: "EXPORT_CAVE_COMPASS"
                QQ.PropertyChanges {
                    fileDialog {
                        title: "Export " + exportManager.currentCaveName + " to compass"
                        nameFilters: ["Compass (*.dat)"]
                        fileMode: FileDialog.SaveFile
                        onAccepted: {
                            RootData.lastDirectory = selectedFile
                            exportManager.exportCaveToCompass(selectedFile);
                        }
                    }
                }
            },
            QQ.State {
                name: "EXPORT_CAVE_CHIPDATA"
                QQ.PropertyChanges {
                    fileDialog {
                        title: "Export " + exportManager.currentCaveName + " to chipdata"
                        nameFilters: ["Chipdata (*.*)"]
                        fileMode: FileDialog.SaveFile
                        onAccepted: {
                            exportManager.exportCaveToChipdata(selectedFile);
                        }
                    }
                }
            },
            QQ.State {
                name: "IMPORT_COMPASS"
                QQ.PropertyChanges {
                    fileDialog {
                        title: "Import from Compass"
                        nameFilters: ["Compass (*.dat)"]
                        fileMode: FileDialog.OpenFile
                        onAccepted: {
                            RootData.lastDirectory = selectedFile
                            RootData.surveyImportManager.importCompassDataFile(selectedFiles);
                        }
                    }
                }
            }
        ]
    }

    ColumnLayout {
        id: rowId

        spacing: 3

        Controls.Button {
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

        Controls.Button {
            id: importButton

            objectName: "importButton"

            text: "Import"
            visible: false

            onClicked: {
                importContextMenu.popup()
            }

            Controls.Menu {
                id: importContextMenu

                Controls.MenuItem {
                    text: "Survex (.svx)"
                    onTriggered: RootData.surveyImportManager.importSurvex()
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
                    onTriggered: RootData.surveyImportManager.importWalls()
                }

                Controls.MenuItem {
                    text: "Walls (.srv)"
                    onTriggered: RootData.surveyImportManager.importWallsSrv()
                }

                Controls.MenuItem {
                    text: "CSV (.csv)"
                    objectName: "csvMenuItem"
                    onTriggered: {
                        var oldImportExport = iconBar.page.childPage("CSV Import");
                        if(oldImportExport !== RootData.pageSelectionModel.currentPage) {
                            if(oldImportExport !== null) {
                                RootData.pageSelectionModel.unregisterPage(oldImportExport);
                            }

                            var page = RootData.pageSelectionModel.registerPage(iconBar.page,
                                                                                "CSV Importer",
                                                                                csvImporterComponent,
                                                                                {});
                            RootData.pageSelectionModel.gotoPage(page);
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
