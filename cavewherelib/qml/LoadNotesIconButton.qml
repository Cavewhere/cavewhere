/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib
import QtQuick.Dialogs

NeutralIconButton {
    id: buttonId

    signal filesSelected(list<url> images)

    iconSource: "qrc:icons/svg/addNotes.svg"
    text: "Load"

    onClicked: {
        fileDialog.open();
    }

    FileDialog {
        id: fileDialog;
        nameFilters: [ "All (" + RootData.supportImageFormats + " *.glb)"];
        title: "Load Images or LiDAR scans"
        // selectMultiple: true
        fileMode: FileDialog.OpenFiles
        currentFolder: RootData.lastDirectory
        onAccepted: {
            RootData.lastDirectory = fileDialog.currentFolder
            buttonId.filesSelected(fileDialog.selectedFiles)
        }
    }

    ImageValidator {
        id: imageValidator
    }
}
