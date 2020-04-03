/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import Cavewhere 1.0

DataTabWidget {
    id: tripTabWidget
    property Trip currentTrip:  null


//    Text {
//        property string label: "Overview"
//        property string icon:  "qrc:icons/dataOverview.png"
//        text: "This is the Trip overview page"
//    }

    //Comment this out, because it's slow
    SurveyEditor {
        property string label: "Data"
        property string icon: "qrc:icons/data.png"

        currentTrip: tripTabWidget.currentTrip === null ? defaultTrip : tripTabWidget.currentTrip
        //    text: "This is the Data page"
    }

    NotesGallery {
        property string label: "Notes"
        property string icon: "qrc:icons/notes.png"

        notesModel: tripTabWidget.currentTrip !== null ? tripTabWidget.currentTrip.notes : null

        onImagesAdded: {
            currentTrip.notes.addFromFiles(images);
        }

    }


//    Text {
//        property string label: "Pictures"
//        property string icon: "qrc:icons/pictures.png"
//        text: "This is the Pictures page"
//    }

//    Text {
//        property string label: "Log"
//        property string icon: "qrc:icons/log.png"
//        text: "This is the Log page"
//    }

}
