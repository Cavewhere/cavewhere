/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0
import QtQuick.Controls 1.2 as Controls;
import QtQuick.Dialogs 1.2

Item {
    id: iconBar

    property alias currentTrip: exportManager.trip
    property alias currentCave: exportManager.cave
    property alias currentRegion: exportManager.cavingRegion

    implicitWidth: rowId.width
    implicitHeight: rowId.height

    SurveyExportManager {
        id: exportManager
    }

    Item {
        id: fileDialogItem
        FileDialog {
            id: fileDialog
        }

        states: [
            State {
                name: "EXPORT_TRIP_SURVEX"
                PropertyChanges {
                    target: fileDialog
                    title: "Export " + exportManager.currentTripName + " to survex"
                    nameFilters: ["Survex (*.svx)"]
                    selectExisting: false
                    onAccepted: {
                        exportManager.exportSurvexTrip(fileUrl);
                    }
                }
            },
            State {
                name: "EXPORT_CAVE_SURVEX"
                PropertyChanges {
                    target: fileDialog
                    title: "Export " + exportManager.currentCaveName + " to survex"
                    nameFilters: ["Survex (*.svx)"]
                    selectExisting: false
                    onAccepted: {
                        exportManager.exportSurvexCave(fileUrl);
                    }
                }
            },
            State {
                name: "EXPORT_REGION_SURVEX"
                PropertyChanges {
                    target: fileDialog
                    title: "Export All Caves to survex"
                    nameFilters: ["Survex (*.svx)"]
                    selectExisting: false
                    onAccepted: {
                        exportManager.exportSurvexRegion(fileUrl);
                    }
                }
            },
            State {
                name: "EXPORT_CAVE_COMPASS"
                PropertyChanges {
                    target: fileDialog
                    title: "Export " + exportManager.currentCaveName + " to compass"
                    nameFilters: ["Compass (*.dat)"]
                    selectExisting: false
                    onAccepted: {
                        exportManager.exportCaveToCompass(fileUrl);
                    }
                }
            }
        ]
    }

    Row {
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
                    onTriggered: rootData.surveyImportManager.importSurvex()
                }

                Controls.MenuItem {
                    text: "Compass (.dat)"
                    onTriggered: rootData.surveyImportManager.importCompassDataFile();
                }
            }
        }
    }
}
