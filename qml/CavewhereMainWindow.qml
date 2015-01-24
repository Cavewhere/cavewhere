/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
//import Cavewhere 1.0
//import QtGraphicalEffects 1.0
import QtQuick.Controls 1.0
import QtQuick.Window 2.0
import QtQuick.Dialogs 1.2

ApplicationWindow {
    id: applicationWindow
    objectName: "applicationWindow"

    visible: license.hasReadLicenseAgreement
    width: 1280
    height: 800
    title: "Cavewhere - " + version
    //    anchors.fill: parent;

    LicenseWindow { }

    Item {
        id: rootQMLItem
        anchors.fill: parent
    }

    menuBar: FileButtonAndMenu {
        id: fileMenuButton

//        terrainRenderer: terrainRendererId
//        dataPage: loadAboutWindowId.item.dataPage //dataMainPageId
        mainContentLoader: loadMainContentsId
        saveAsFileDialog: saveAsFileDialogId
        loadFileDialog: loadFileDialogId

        onOpenAboutWindow:  {
            loadAboutWindowId.setSource("AboutWindow.qml")
        }
    }

    Loader {
        id: loadAboutWindowId
    }

    Loader {
        id: loadMainContentsId
        source: "MainContent.qml"
        anchors.fill: parent
        asynchronous: false //FIXME: Once https://bugreports.qt-project.org/browse/QTBUG-36410 is fixed turn this to true
        visible: status == Loader.Ready

//        onLoaded: {
//            fileMenuButton.dataPage = item.dataPage
//        }
    }

    Column {
        anchors.centerIn: parent
        visible: loadAboutWindowId.status != Loader.Ready

        Text {
            text: {
                switch(loadMainContentsId.status) {
                case Loader.Error:
                    return "QML error in " + loadMainContentsId.source + "<br> check commandline for details";
                case Loader.Loading:
                    return "Loading"
                }
                return "";
            }
        }

        ProgressBar {
            minimumValue: 0
            maximumValue: 100
            value: loadMainContentsId.progress * 100.
            visible: loadMainContentsId.status == Loader.Loading
        }
    }

    FileDialog {
        id: saveAsFileDialogId
        nameFilters: ["Cavewhere Project (*.cw)"]
        title: "Save Cavewhere Project As"
        selectExisting: false
        onAccepted: {
            project.saveAs(fileUrl)
        }
    }

    FileDialog {
        id: loadFileDialogId
        nameFilters: ["Cavewhere File (*.cw)"]
        onAccepted: {
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

    //All the dialogs in cavewher are parented under this item.
    GlobalDialogHandler {
        id: globalDialogHandler
    }



    Component.onCompleted: {
        eventRecorderModel.rootEventObject = applicationWindow
    }
}

