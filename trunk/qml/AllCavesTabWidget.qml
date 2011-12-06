import QtQuick 1.1

DataTabWidget {
    Text {
        property string label: "Overview"
        property string icon:  "qrc:icons/dataOverview.png"
        text: "This is the overview of all the caves"
    }

    Text {
        property string label: "Connections"
        property string icon: "qrc:icons/data.png"
        text: "This is the Connection page"
    }

//    Text {
//        property string label: "Calibrations"
//        property string icon: "qrc:icons/calibration.png"
//        text: "This is the Team page"
//    }
}
