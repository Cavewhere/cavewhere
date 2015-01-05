/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0

IconButton {
    id: buttonId

    signal filesSelected(var images)

    iconSource: "qrc:icons/addNotes.png"
    sourceSize: mainToolBar.iconSize
    text: "Load"

    onClicked: {
        fileDialog.open();
    }

    FileDialogHelper {
        id: fileDialog;
        filter: "Images (*.png *.jpg *.jpeg *.jp2 *.tiff)"
        multipleFiles: true
        settingKey: "lastNoteGalleryImageLocation"
        caption: "Load Images"
        onFilesSelected: {
            var validImages = imageValidator.validateImages(selected);
            buttonId.filesSelected(validImages)
        }
    }

    ImageValidator {
        id: imageValidator
    }
}
