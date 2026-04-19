/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QC.RoundButton {
    id: buttonId

    property SurveyNotesConcatModel notesModel

    signal imageRequested(list<url> images)
    signal sketchRequested()
    signal lidarRequested(list<url> files)

    icon.source: "qrc:twbs-icons/icons/plus.svg"
    icon.width: Theme.iconSizeSmall
    icon.height: Theme.iconSizeSmall

    onClicked: menuId.popup()

    QC.Menu {
        id: menuId
        objectName: "addNoteMenu"

        QC.MenuItem {
            objectName: "imageMenuItem"
            text: "Image"
            onTriggered: imageFileDialogId.open()
        }

        QC.MenuItem {
            objectName: "sketchMenuItem"
            text: "Sketch"
            onTriggered: {
                if (buttonId.notesModel !== null) {
                    buttonId.notesModel.addSketch(Sketch.Plan)
                }
                buttonId.sketchRequested()
            }
        }

        QC.MenuItem {
            objectName: "lidarMenuItem"
            text: "LiDAR"
            onTriggered: lidarFileDialogId.open()
        }
    }

    NotesFileDialog {
        id: imageFileDialogId
        onFilesSelected: (images) => {
            if (buttonId.notesModel !== null) {
                buttonId.notesModel.addFiles(images)
            }
            buttonId.imageRequested(images)
        }
    }

    NotesFileDialog {
        id: lidarFileDialogId
        onFilesSelected: (files) => {
            if (buttonId.notesModel !== null) {
                buttonId.notesModel.addFiles(files)
            }
            buttonId.lidarRequested(files)
        }
    }
}
