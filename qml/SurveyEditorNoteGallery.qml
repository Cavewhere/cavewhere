import QtQuick 2.0
import Cavewhere 1.0

/**
  This Component binds survey editor and NoteGallery together, so the page selection model
  works correctly.
  */

NotesGallery {
    id: noteGallery

    property var editor; //This should be a SurveyEditor
    property var previousState

    readonly property string mode: {
        switch(state) {
        case "": return "DEFAULT"
        case "NO_NOTES": return "DEFAULT"
        case "CARPET": return "CARPET"
        case "SELECT": return "CARPET"
        case "ADD-STATION": return "CARPET"
        case "ADD-SCRAP": return "CARPET"
        default: return "ERROR"
        }
    }

    function setMode(mode) {
        if(mode !== noteGallery.mode) {
            switch(mode) {
            case "DEFAULT":
                noteGallery.state = "CARPET"
                if(notesModel.rowCount() === 0) {
                    noteGallery.state = "NO_NOTES"
                } else {
                    noteGallery.state = ""
                }
                break;
            case "CARPET":
                noteGallery.state = "SELECT"
            }
        }
    }

//    onModeChanged: {
//        console.log("Mode changed" + mode)
////        rootData.pageSelectionModel.gotoPageByName()
//    }

//    onNewStateChanged: {
//        console.log("New State changed:" + newState)
//        state = newState;

//        if(previousState)

//        switch(newState) {
//        case NO_NOTES
//        }
//    }

    onStateChanged: {
        console.log("Statet changed:" + state);
    }

    Component.onCompleted: {
        previousState = state
    }
}

