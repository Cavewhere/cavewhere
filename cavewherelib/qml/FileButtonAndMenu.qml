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

    FileMenu {
        mainContentLoader: menuBarId.mainContentLoader
        loadFileDialog: menuBarId.loadFileDialog
        saveAsFileDialog: menuBarId.saveAsFileDialog
        applicationWindow: menuBarId.applicationWindow
        askToSaveDialog: menuBarId.askToSaveDialog
    }
}
