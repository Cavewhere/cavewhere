/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib
import QtQuick.Dialogs

IconButton {
    id: buttonId

    signal filesSelected(list<url> images)

    iconSource: "qrc:icons/addNotes.png"
    text: "Load"

    onClicked: {
        fileDialog.open();
    }

    FileDialog {
        id: fileDialog;
        nameFilters: [ "Images (" + RootData.supportImageFormats + ")" ];
        title: "Load Images"
        // selectMultiple: true
        currentFolder: RootData.lastDirectory
        onAccepted: {
            RootData.lastDirectory = selectedFiles
            buttonId.filesSelected(fileDialog.selectedFiles)
        }
    }

    ImageValidator {
        id: imageValidator
    }
}