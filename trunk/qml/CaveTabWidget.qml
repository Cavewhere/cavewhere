import QtQuick 1.0

DataTabWidget {
    CaveOverviewPage {
        property string label: "Overview"
        property string icon:  "icons/dataOverview.png"
    }

    Text {
        property string label: "Notes"
        property string icon: "icons/notes.png"
        text: "This is the Notes page"
    }

    Text {
        property string label: "Team"
        property string icon: "icons/team.png"
        text: "This is the Team page"
    }

    Text {
        property string label: "Pictures"
        property string icon: "icons/pictures.png"
        text: "This is the Pictures page"
    }

    Text {
        property string label: "Log"
        property string icon: "icons/log.png"
        text: "This is the Log page"
    }
}
