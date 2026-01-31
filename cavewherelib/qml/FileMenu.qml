pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Dialogs
import cavewherelib

QC.Menu {
    id: fileMenuId

    property QQ.Loader mainContentLoader
    required property FileDialog loadFileDialog
    required property FileDialog saveAsFileDialog
    required property QC.ApplicationWindow applicationWindow
    required property AskToSaveDialog askToSaveDialog

    title: "File"

    // Resize the window to the provided aspect ratio and largest dimension without exceeding the screen.
    function resizeTo(widthRatio, heightRatio, largestDimPixels)
    {
        const devicePixelRatio = applicationWindow.screen.devicePixelRatio || 1;
        const titleBarHeightPx = (RootData.titleBarHeight || 0) * devicePixelRatio;
        const baseScale = largestDimPixels / Math.max(widthRatio, heightRatio);

        let targetWidthPx = widthRatio * baseScale;
        let targetHeightPx = heightRatio * baseScale;

        // Fit to screen while preserving the aspect ratio.
        const fitScale = Math.min(
                    1,
                    applicationWindow.screen.width / targetWidthPx,
                    (applicationWindow.screen.height - titleBarHeightPx) / targetHeightPx);

        targetWidthPx *= fitScale;
        targetHeightPx *= fitScale;

        applicationWindow.width = Math.max(0, targetWidthPx / devicePixelRatio);
        applicationWindow.height = Math.max(0, (targetHeightPx - titleBarHeightPx) / devicePixelRatio);

        // console.log("Ratio:" + (applicationWindow.width / (applicationWindow.height + titleBarHeightPx)) + " " + (widthRatio / heightRatio))
    }

    QC.Action {
        id: newActionId
        text: "&New"
        shortcut: "Ctrl+N"
        onTriggered: {
            fileMenuId.askToSaveDialog.taskName = "creating a new file"
            fileMenuId.askToSaveDialog.afterSaveFunc = function() {
                RootData.newProject();
            }
            fileMenuId.askToSaveDialog.askToSave();
        }
    }

    QC.Action {
        id: openActionId
        text: "&Open"
        shortcut: "Ctrl+O"
        onTriggered: {
            const openDialog = function() {
                loadFileDialog.open();
            }

            fileMenuId.askToSaveDialog.taskName = "opening a project";
            fileMenuId.askToSaveDialog.afterSaveFunc = openDialog;
            fileMenuId.askToSaveDialog.askToSave();
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
                fileMenuId.saveAsFileDialog.open();
            }
        }
    }

    QC.MenuItem {
        id: saveAsMenuItem
        text: "Save As"
        onTriggered:{
            fileMenuId.saveAsFileDialog.open()
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

    QC.MenuSeparator {}

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
                resizeTo(16, 9, 1920)
            }
        }

        QC.MenuItem {
            text: "Resize to 1080p - Vertical"
            onTriggered: {
                resizeTo(9, 16, 1920)
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

    QC.Action {
        id: quitActionId
        text: "Quit"
        shortcut: "Ctrl+Q"
        onTriggered: {
            fileMenuId.applicationWindow.close(); // or Qt.quit();
        }
    }
}
