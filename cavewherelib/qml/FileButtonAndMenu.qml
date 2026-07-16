/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick.Controls as QC
import QtQuick.Dialogs
import cavewherelib

QC.MenuBar {
    id: menuBarId
    property FileDialog loadFileDialog;
    property SaveAsDialog saveAsFileDialog;
    property QC.ApplicationWindow applicationWindow;
    property AskToSaveDialog askToSaveDialog;

    signal openAboutWindow;

    FileMenu {
        loadFileDialog: menuBarId.loadFileDialog
        saveAsFileDialog: menuBarId.saveAsFileDialog
        applicationWindow: menuBarId.applicationWindow
        askToSaveDialog: menuBarId.askToSaveDialog
    }
}
