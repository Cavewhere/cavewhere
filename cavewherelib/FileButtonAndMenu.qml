/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick as QQ
import QtQuick.Controls
import QtQuick.Dialogs
import cavewherelib

MenuBar {
    id: menuBarId
    // property var terrainRenderer; //For taking screenshots
//    property var dataPage; //Should be a DataMainPage
    property QQ.Loader mainContentLoader;
    property FileDialog loadFileDialog;
    property FileDialog saveAsFileDialog;
    property ApplicationWindow applicationWindow;
    property AskToSaveDialog askToSaveDialog;

    signal openAboutWindow;

    Menu {
        title: "File"

        Action {
            id: newActionId
            text: "&New"
            shortcut: "Ctrl+N"
            onTriggered: {
                menuBarId.askToSaveDialog.taskName = "creating a new file"
                menuBarId.askToSaveDialog.afterSaveFunc = function() {
                    RootData.pageSelectionModel.clearHistory()
                    RootData.pageSelectionModel.gotoPageByName(null, "View")
                    project.newProject();
                }
                menuBarId.askToSaveDialog.askToSave();
            }
        }

        Action {
            id: openActionId
            text: "&Open"
            shortcut: "Ctrl+O"
            onTriggered: {
                menuBarId.askToSaveDialog.taskName = "opening";
                menuBarId.askToSaveDialog.afterSaveFunc = function() {
                    loadFileDialog.open()
                }
                menuBarId.askToSaveDialog.askToSave();
            }
        }

        MenuSeparator {}

        Action {
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

        MenuItem {
            id: saveAsMenuItem
            text: "Save As"
            onTriggered:{
                menuBarId.saveAsFileDialog.open()
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Settings"
            onTriggered: {
                RootData.pageSelectionModel.gotoPageByName(null, "Settings");
            }
        }

        MenuItem {
            text: "About"
            onTriggered: {
                RootData.pageSelectionModel.gotoPageByName(null, "About");
            }
        }
        Action {
            id: quitActionId
            text: "Quit"
            shortcut: "Ctrl+Q"
            onTriggered: {
                menuBarId.applicationWindow.close(); // or Qt.quit();
            }
        }
    }

    Menu {
        title: "Debug"

        MenuItem {
            text: "Compute Scraps"
            onTriggered: RootData.scrapManager.updateAllScraps()
        }

        // MenuItem {
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

        MenuItem {
            text: "Scraps Visible"
            checked: RootData.regionSceneManager.scraps.visible
            checkable: true
            onTriggered: {
                RootData.regionSceneManager.scraps.visible = !RootData.regionSceneManager.scraps.visible
                // terrainRenderer.update()
            }
        }

        MenuItem {
            text: "Leads Visible"
            checked: RootData.leadsVisible
            checkable: true
            onTriggered: {
                RootData.leadsVisible = !RootData.leadsVisible
            }
        }


        MenuItem {
            text: "Station Labels Visible"
            checked: RootData.stationsVisible
            checkable: true
            onTriggered: {
                RootData.stationsVisible = !RootData.stationsVisible
            }
        }

        MenuItem {
            text: "Testcases"
            onTriggered: {
                RootData.pageSelectionModel.gotoPageByName(null, "Testcases");
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
