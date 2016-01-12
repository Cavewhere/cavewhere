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
    property alias importVisible: importButton.visible

    implicitWidth: rowId.width
    implicitHeight: rowId.height

    SurveyExportManager {
        id: exportManager
    }

    SurveyImportManager {
        id: importManager
        cavingRegion: iconBar.currentRegion
        undoStack: rootData.undoStack
        onMessageAdded: {
            messageListDialog.visible = true;
            messageListDialog.messages.append({
                                                  severity: severity,
                                                  message: message,
                                                  lineCount: message.split('\n').length,
                                                  source: source,
                                                  startLine: startLine + 1,
                                                  startColumn: startColumn + 1
                                              });
            messageListDialog.scrollToBottom();
        }
        onMessagesCleared: messageListDialog.messages.clear()
    }

    MessageListDialog {
        visible: false
        modality: Qt.NonModal
        id: messageListDialog
        width: 730
        height: 400
        font: importManager.messageListFont
    }

    Item {
        id: fileDialogItem
        FileDialog {
            id: fileDialog
            folder: rootData.lastDirectory
        }

        states: [
            State {
                name: "EXPORT_TRIP_SURVEX"
                PropertyChanges {
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
            State {
                name: "EXPORT_CAVE_SURVEX"
                PropertyChanges {
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
            State {
                name: "EXPORT_REGION_SURVEX"
                PropertyChanges {
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
            State {
                name: "EXPORT_CAVE_COMPASS"
                PropertyChanges {
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
            State {
                name: "IMPORT_COMPASS"
                PropertyChanges {
                    target: fileDialog
                    title: "Import from Compass"
                    nameFilters: ["Compass (*.dat)"]
                    selectExisting: true
                    selectMultiple: true
                    onAccepted: {
                        rootData.lastDirectory = fileUrl
                        messageListDialog.title = "Compass Import"
                        importManager.importCompassDataFile(fileUrls);
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
            }
        }
    }
}
