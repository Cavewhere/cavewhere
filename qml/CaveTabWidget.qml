import QtQuick 1.1

DataTabWidget {
    property variant currentCave: null

    CaveOverviewPage {
        property string label: "Overview"
        property string icon:  "qrc:icons/dataOverview.png"
    }

    Text {
        property string label: "Notes"
        property string icon: "qrc:icons/notes.png"
        text: "This is the Notes page"
    }

    Text {
        property string label: "Team"
        property string icon: "qrc:icons/team.png"
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
