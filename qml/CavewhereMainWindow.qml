/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import QtQuick.Controls
import QtQuick.Window 2.0
import QtQuick.Dialogs
import Cavewhere 1.0;
import Qt.labs.settings 1.1

ApplicationWindow {
    id: applicationWindowId
    objectName: "applicationWindow"

    visible: false
    width: 1024
    height: 576

    title: {
        var baseName = "CaveWhere - " + version
        var filename = rootData.project.isTemporaryProject ? "New File" : rootData.project.filename;
        return baseName + "   " + filename
    }

    menuBar: FileButtonAndMenu {
        id: fileMenuButton

//        terrainRenderer: terrainRendererId
//        dataPage: loadAboutWindowId.item.dataPage //dataMainPageId
        mainContentLoader: loadMainContentsId
        saveAsFileDialog: saveAsFileDialogId
        loadFileDialog: loadFileDialogId
        applicationWindow: applicationWindowId
        askToSaveDialog: askToSaveDialogId

        onOpenAboutWindow:  {
            loadAboutWindowId.setSource("AboutWindow.qml")
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
        id: loadMainContentsId
        source: "MainContent.qml"
        anchors.fill: parent
        asynchronous: true
        visible: status == QQ.Loader.Ready

//        onLoaded: {
//            fileMenuButton.dataPage = item.dataPage
//        }
    }

    QQ.Column {
        anchors.centerIn: parent
        visible: loadAboutWindowId.status != QQ.Loader.Ready

        Text {
            text: {
                switch(loadMainContentsId.status) {
                case QQ.Loader.Error:
                    return "QML error in " + loadMainContentsId.source + "<br> check commandline for details";
                case QQ.Loader.Loading:
                    return "Loading"
                }
                return "";
            }
        }

        ProgressBar {
            from: 0
            to: 100
            value: loadMainContentsId.progress * 100.
            visible: loadMainContentsId.status == QQ.Loader.Loading
        }
    }

    SaveAsDialog {
        id: saveAsFileDialogId
    }

    ErrorDialog {
        id: projectErrorDialog
        model: rootData.project.errorModel
    }

    FileDialog {
        id: loadFileDialogId
        nameFilters: ["CaveWhere File (*.cw)"]
        currentFolder: rootData.lastDirectory
        onAccepted: {
            rootData.lastDirectory = fileUrl
            rootData.pageSelectionModel.clearHistory();
            rootData.pageSelectionModel.gotoPageByName(null, "View")
            rootData.project.loadFile(fileUrl)
        }
    }

    //There's only one shadow input text editor for the cavewhere program
    //This make the input creation much faster for any thing that needs an editor
    //Only one editor can be open at a time
    GlobalShadowTextInput {
        id: globalShadowTextInput
    }

    //All the dialogs in cavewhere are parented under this item. This prevents
    //mouse clicks outside of the dialog
    GlobalDialogHandler {
        id: globalDialogHandler
    }

    QQ.Item {
        id: rootPopupItem
        anchors.fill: parent
    }

    AskToSaveDialog {
        id: askToSaveDialogId
        saveAsDialog: saveAsFileDialogId
    }

    SaveFeedbackHelpBox {
        anchors.centerIn: parent
    }

    onClosing: {
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

