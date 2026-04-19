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

QQ.Item {
    id: rootId

    property SurveyNotesConcatModel notesModel

    signal filesRequested(list<url> files)
    signal sketchRequested()

    function popup() {
        menuId.popup()
    }

    QC.Menu {
        id: menuId
        objectName: "addNoteMenu"

        QC.MenuItem {
            objectName: "filesMenuItem"
            text: "Notes or 3D Model"
            onTriggered: filesDialogId.open()
        }

        QC.MenuItem {
            objectName: "sketchMenuItem"
            text: "Sketch"
            onTriggered: {
                if (rootId.notesModel !== null) {
                    rootId.notesModel.addSketch(Sketch.Plan)
                }
                rootId.sketchRequested()
            }
        }
    }

    NotesFileDialog {
        id: filesDialogId
        onFilesSelected: (files) => {
            if (rootId.notesModel !== null) {
                rootId.notesModel.addFiles(files)
            }
            rootId.filesRequested(files)
        }
    }
}
