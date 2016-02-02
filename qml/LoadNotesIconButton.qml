/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0
import QtQuick.Dialogs 1.2

IconButton {
    id: buttonId

    signal filesSelected(var images)

    iconSource: "qrc:icons/addNotes.png"
    sourceSize: mainToolBar.iconSize
    text: "Load"

    onClicked: {
        fileDialog.open();
    }

    FileDialog {
        id: fileDialog;
        nameFilters: "Images (*.png *.jpg *.jpeg *.jp2 *.tiff)"
        title: "Load Images"
        selectMultiple: true
        folder: rootData.lastDirectory
        onAccepted: {
            rootData.lastDirectory = fileUrl
            buttonId.filesSelected(fileDialog.fileUrls)
        }
    }

    ImageValidator {
        id: imageValidator
    }
}
