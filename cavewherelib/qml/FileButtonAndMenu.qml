/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Dialogs
import cavewherelib

QC.MenuBar {
    id: menuBarId
    // property var terrainRenderer; //For taking screenshots
//    property var dataPage; //Should be a DataMainPage
    property QQ.Loader mainContentLoader;
    property FileDialog loadFileDialog;
    property FileDialog saveAsFileDialog;
    property QC.ApplicationWindow applicationWindow;
    property AskToSaveDialog askToSaveDialog;

    signal openAboutWindow;

    QC.Menu {
        title: "File"

        QC.Action {
            id: newActionId
            text: "&New"
            shortcut: "Ctrl+N"
            onTriggered: {
                menuBarId.askToSaveDialog.taskName = "creating a new file"
                menuBarId.askToSaveDialog.afterSaveFunc = function() {
                    RootData.newProject();
                }
                menuBarId.askToSaveDialog.askToSave();
            }
        }

        QC.Action {
            id: openActionId
            text: "&Open"
            shortcut: "Ctrl+O"
            onTriggered: {
                const openDialog = function() {
                    RootData.pageSelectionModel.currentPageAddress = "Source";
                    loadFileDialog.open();
                }

                menuBarId.askToSaveDialog.taskName = "opening a project";
                menuBarId.askToSaveDialog.afterSaveFunc = openDialog;
                menuBarId.askToSaveDialog.askToSave();
            }
        }

        QC.MenuSeparator {}

        QC.Action {
            id: saveActionId
            text: "&Save"
            shortcut: "Ctrl+S"
            onTriggered: {
                if (RootData.project.canSaveDirectly) {
                    RootData.project.save();
                } else {
                    menuBarId.saveAsFileDialog.open();
                }
            }
        }

        QC.MenuItem {
            id: saveAsMenuItem
            text: "Save As"
            onTriggered:{
                menuBarId.saveAsFileDialog.open()
            }
        }

        QC.MenuSeparator {}

        QC.MenuItem {
            text: "Settings"
            onTriggered: {
                RootData.pageSelectionModel.gotoPageByName(null, "Settings");
            }
        }

        QC.MenuItem {
            text: "About"
            onTriggered: {
                RootData.pageSelectionModel.gotoPageByName(null, "About");
            }
        }
        QC.Action {
            id: quitActionId
            text: "Quit"
            shortcut: "Ctrl+Q"
            onTriggered: {
                menuBarId.applicationWindow.close(); // or Qt.quit();
            }
        }
    }

    QC.Menu {
        title: "Debug"

        QC.MenuItem {
            text: "Compute Scraps"
            onTriggered: RootData.scrapManager.updateAllScraps()
        }

        // QC.MenuItem {
        //     text: "Reload"
        //     shortcut: "Ctrl+R"
        //     onTriggered: {
        //         //Keep the current address for the current page
        //         var currentAddress = RootData.pageSelectionModel.currentPageAddress;
        //         RootData.pageSelectionModel.clear();

        //         var currentSource = mainContentLoader.source;
        //         mainContentLoader.source = ""
        //         mainContentLoader.asynchronous = false;
        //         qmlReloader.reload();
        //         mainContentLoader.source = currentSource;

        //         console.log("Loader status:" + mainContentLoader.status + "Loader:" + QQ.Loader.Ready + " " + currentAddress)

        //         RootData.pageSelectionModel.currentPageAddress = currentAddress;
        //     }
        // }

        QC.MenuItem {
            text: "Scraps Visible"
            checked: RootData.regionSceneManager.scraps.visible
            checkable: true
            onTriggered: {
                RootData.regionSceneManager.items.visible = !RootData.regionSceneManager.items.visible
                // terrainRenderer.update()
            }
        }

        QC.MenuItem {
            text: "Leads Visible"
            checked: RootData.leadsVisible
            checkable: true
            onTriggered: {
                RootData.leadsVisible = !RootData.leadsVisible
            }
        }


        QC.MenuItem {
            text: "Station Labels Visible"
            checked: RootData.stationsVisible
            checkable: true
            onTriggered: {
                RootData.stationsVisible = !RootData.stationsVisible
            }
        }

        QC.MenuItem {
            text: "Testcases"
            onTriggered: {
                RootData.pageSelectionModel.gotoPageByName(null, "Testcases");
            }
        }

        QC.MenuItem {
            text: "Resize to 1080p"
            onTriggered: {
                let titleBarHeight = RootData.titleBarHeight * applicationWindow.screen.devicePixelRatio
                applicationWindow.width = 1920 / applicationWindow.screen.devicePixelRatio
                applicationWindow.height = (1080 - titleBarHeight) / applicationWindow.screen.devicePixelRatio
            }
        }

        // Menu {
        //     title: "Event Recording"

        //     MenuItem {
        //         text: eventRecorderModel.recording ? "Stop" : "Start"
        //         shortcut: "Ctrl+P"
        //         onTriggered: {
        //             if(eventRecorderModel.recording) {
        //                 eventRecorderModel.stopRecording()
        //             } else {
        //                 eventRecorderModel.startRecording()
        //             }
        //         }
        //     }

        //     MenuItem {
        //         text: "Play Previous Recording"
        //         shortcut: "Ctrl+."
        //         enabled: !eventRecorderModel.recording
        //         onTriggered: {
        //             eventRecorderModel.replayLastRecording();
        //         }
        //     }
        // }
    }
}
