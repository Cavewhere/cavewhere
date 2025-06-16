/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Dialogs
import cavewherelib
import QQuickGit

ApplicationWindow {
    id: applicationWindowId
    objectName: "applicationWindow"

    visible: false
    width: 1024
    height: 576
    // flags: Qt.FramelessWindowHint

    title: {
        var baseName = "CaveWhere - " + RootData.version
        var filename = RootData.project.isTemporaryProject ? "New File" : RootData.project.filename;
        return baseName + "   " + filename
    }

    menuBar: fileMenuLoaderId.item

    QQ.Loader {
        id: fileMenuLoaderId
        active: RootData.desktopBuild && RootData.account.isValid
        sourceComponent: FileButtonAndMenu {
            id: fileMenuButton

            saveAsFileDialog: saveAsFileDialogId
            loadFileDialog: loadFileDialogId
            applicationWindow: applicationWindowId
            askToSaveDialog: askToSaveDialogId

            onOpenAboutWindow:  {
                loadAboutWindowId.setSource("AboutWindow.qml")
            }
        }
    }

    ScreenSizeSaver {
        id: screenSizeSaverId
        window: applicationWindowId
        windowName: "mainWindow"
    }

    QQ.Loader {
        id: loadAboutWindowId
    }

    QQ.Loader {
        active: RootData.account.isValid
        anchors.fill: parent
        sourceComponent: MainContent {
            anchors.fill: parent
        }
    }

    QQ.Loader {
        active: !RootData.account.isValid
        anchors.fill: parent
        sourceComponent: WelcomePage {
            anchors.fill: parent
            account: RootData.account
        }
    }

    QQ.Item {
        id: overlayItem
        anchors.fill: parent
    }

    SaveAsDialog {
        id: saveAsFileDialogId
    }

    ErrorDialog {
        id: projectErrorDialog
        model: RootData.project.errorModel
    }

    FileDialog {
        id: loadFileDialogId
        nameFilters: ["CaveWhere File (*.cw)"]
        currentFolder: RootData.lastDirectory
        onAccepted: {
            RootData.lastDirectory = selectedFile
            RootData.pageSelectionModel.clearHistory();
            RootData.pageSelectionModel.gotoPageByName(null, "View")
            RootData.project.loadFile(selectedFile)
        }
    }

    //There's only one shadow input text editor for the cavewhere program
    //This make the input creation much faster for any thing that needs an editor
    //Only one editor can be open at a time
    //THIS IS NOW A SINGLETON
    // GlobalShadowTextInput {
    //     id: globalShadowTextInput
    // }

    //All the dialogs in cavewhere are parented under this item. This prevents
    //mouse clicks outside of the dialog
    // GlobalDialogHandler {
    //     id: globalDialogHandler
    // }

    //Use RootPopupItem instead. A signleton
    // QQ.Item {
    //     id: rootPopupItem
    //     anchors.fill: parent
    // }

    AskToSaveDialog {
        id: askToSaveDialogId
        saveAsDialog: saveAsFileDialogId
        taskName: "quiting"
    }

    SaveFeedbackHelpBox {
        anchors.centerIn: parent
    }

    onClosing: (close) => {
        askToSaveDialogId.taskName = "quiting"
        askToSaveDialogId.afterSaveFunc = function() {
            Qt.quit();
        }

        askToSaveDialogId.askToSave();
        close.accepted = false;
    }

    QQ.Component.onCompleted: {
        screenSizeSaverId.resize();
    }
}

