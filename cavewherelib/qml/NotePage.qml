/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

StandardPage {
    id: notePage

    objectName: "notePage"

    property int currentNoteIndex: 0
    property SurveyNotesConcatModel notesModel: null

    PageView.defaultProperties: {
        "notesModel": null,
        "currentNoteIndex": 0
    }

    NotesGallery {
        anchors.fill: parent
        showGallery: false
        notesModel: notePage.notesModel
        currentNoteIndex: notePage.currentNoteIndex
        onImagesAdded: (images) => notePage.notesModel.addFiles(images)
    }
}
