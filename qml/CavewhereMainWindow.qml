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

ApplicationWindow {
    visible: license.hasReadLicenseAgreement
    width: 1000
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
        asynchronous: true
        visible: status == Loader.Ready

        onLoaded: {
            fileMenuButton.dataPage = item.dataPage
            fileMenuButton.terrainRenderer = item.terrainRenderer
        }
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
}

