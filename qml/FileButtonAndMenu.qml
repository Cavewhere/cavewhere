/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 1.0 as QQ // to target S60 5th Edition or Maemo 5
import QtQuick 2.0 as QQ
import QtQuick.Controls 1.0
import QtQuick.Dialogs 1.2

MenuBar {
    id: menuBarId
    property var terrainRenderer; //For taking screenshots
//    property var dataPage; //Should be a DataMainPage
    property QQ.Loader mainContentLoader;
    property FileDialog loadFileDialog;
    property FileDialog saveAsFileDialog;
    property ApplicationWindow applicationWindow;
    property AskToSaveDialog askToSaveDialog;

    signal openAboutWindow;

    Menu {
        title: "File"

        MenuItem {
            text: "New"
            shortcut: "Ctrl+N"
            onTriggered: {
                askToSaveDialog.taskName = "creating a new file"
                askToSaveDialog.afterSaveFunc = function() {
                    rootData.pageSelectionModel.clearHistory()
                    rootData.pageSelectionModel.gotoPageByName(null, "View")
                    project.newProject();
                }
                askToSaveDialog.askToSave();
            }
        }

        MenuItem {
            text: "Open"
            shortcut: "Ctrl+O"
            onTriggered: {
//                dataPage.resetSideBar() //Fixes a crash when a new project is loaded
                askToSaveDialog.taskName = "opening";
                askToSaveDialog.afterSaveFunc = function() {
                    loadFileDialog.open()
                }
                askToSaveDialog.askToSave();
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Save"
            shortcut: "Ctrl+S"
            onTriggered: {
                if(project.canSaveDirectly) {
                    project.save();
                } else {
                    saveAsFileDialog.open()
                }
            }
        }

        MenuItem {
            id: saveAsMenuItem
            text: "Save As"
            onTriggered:{
                saveAsFileDialog.open()
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Settings"
            onTriggered: {
                rootData.pageSelectionModel.gotoPageByName(null, "Settings");
            }
        }

        MenuItem {
            text: "About"
            onTriggered: {
                rootData.pageSelectionModel.gotoPageByName(null, "About");
            }
        }

        MenuItem {
            text: "Quit"
            shortcut: "Ctrl+Q"
            onTriggered: applicationWindow.close(); //Qt.quit()
        }
    }

    Menu {
        title: "Debug"

        MenuItem {
            text: "Compute Scraps"
            onTriggered: scrapManager.updateAllScraps()
        }

        MenuItem {
            text: "Reload"
            shortcut: "Ctrl+R"
            onTriggered: {
                //Keep the current address for the current page
                var currentAddress = rootData.pageSelectionModel.currentPageAddress;
                rootData.pageSelectionModel.clear();

                var currentSource = mainContentLoader.source;
                mainContentLoader.source = ""
                mainContentLoader.asynchronous = false;
                qmlReloader.reload();
                mainContentLoader.source = currentSource;

                console.log("Loader status:" + mainContentLoader.status + "Loader:" + QQ.Loader.Ready + " " + currentAddress)

                rootData.pageSelectionModel.currentPageAddress = currentAddress;
            }
        }

        MenuItem {
            text: "Scraps Visible"
            checked: regionSceneManager.scraps.visible
            checkable: true
            onTriggered: {
                regionSceneManager.scraps.visible = !regionSceneManager.scraps.visible
                terrainRenderer.update()
            }
        }

        MenuItem {
            text: "Leads Visible"
            checked: rootData.leadsVisible
            checkable: true
            onTriggered: {
                rootData.leadsVisible = !rootData.leadsVisible
            }
        }


        MenuItem {
            text: "Station Labels Visible"
            checked: rootData.stationsVisible
            checkable: true
            onTriggered: {
                rootData.stationsVisible = !rootData.stationsVisible
            }
        }

        MenuItem {
            text: "Testcases"
            onTriggered: {
                rootData.pageSelectionModel.gotoPageByName(null, "Testcases");
            }
        }

        Menu {
            title: "Event Recording"

            MenuItem {
                text: eventRecorderModel.recording ? "Stop" : "Start"
                shortcut: "Ctrl+P"
                onTriggered: {
                    if(eventRecorderModel.recording) {
                        eventRecorderModel.stopRecording()
                    } else {
                        eventRecorderModel.startRecording()
                    }
                }
            }

            MenuItem {
                text: "Play Previous Recording"
                shortcut: "Ctrl+."
                enabled: !eventRecorderModel.recording
                onTriggered: {
                    eventRecorderModel.replayLastRecording();
                }
            }
        }
    }
}
