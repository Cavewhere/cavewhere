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
        id: notesGallery
        anchors.fill: parent
        showGallery: false
        isNarrow: true
        notesModel: notePage.notesModel
        onImagesAdded: (images) => notePage.notesModel.addFiles(images)
        onNoteIndexChangeRequested: (index) => notePage.currentNoteIndex = index
    }

    // Imperative sync instead of a binding because ListView internals
    // imperatively reset galleryView.currentIndex (e.g. when the model is
    // assigned), which would permanently break a binding on the aliased
    // property. A change handler always runs regardless of binding state.
    onCurrentNoteIndexChanged: notesGallery.currentNoteIndex = currentNoteIndex
    QQ.Component.onCompleted: notesGallery.currentNoteIndex = currentNoteIndex
}
