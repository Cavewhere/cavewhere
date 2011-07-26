import QtQuick 1.0

DataTabWidget {
    id: tripTabWidget
    property variant currentTrip:  null

    Text {
        property string label: "Overview"
        property string icon:  "qrc:icons/dataOverview.png"
        text: "This is the Trip overview page"
    }

    SurveyEditor {
        property string label: "Data"
        property string icon: "qrc:icons/data.png"

        currentTrip: tripTabWidget.currentTrip != null ? tripTabWidget.currentTrip : null
        //    text: "This is the Data page"
    }

    NotesGallery {
        property string label: "Notes"
        property string icon: "qrc:icons/notes.png"

        notesModel: tripTabWidget.currentTrip != null ? tripTabWidget.currentTrip.notes : null

        onImagesAdded: {
            console.debug("images:" + images);
            project.addNoteImages(tripTabWidget.currentTrip, images);
        }

    }

    Text {
        property string label: "Team"
        property string icon: "qrc:icons/team.png"
        text: "This is the Team page"
    }

    Text {
        property string label: "Calibrations"
        property string icon: "qrc:icons/calibration.png"
        text: "This is the Team page"
    }

    Text {
        property string label: "Pictures"
        property string icon: "qrc:icons/pictures.png"
        text: "This is the Pictures page"
    }

    Text {
        property string label: "Log"
        property string icon: "qrc:icons/log.png"
        text: "This is the Log page"
    }
}
