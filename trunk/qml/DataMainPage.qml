import QtQuick 1.0
import Cavewhere 1.0

Rectangle {
    id: pageId
//    width: 500
//    height: 500

   // anchors.fill: parent

    ProxyWidget {
        id: regionTree

        width: 300;

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: 1

        widget: regionTreeView

        Component.onCompleted: {
            console.debug("Loading Widget: " + widget);
        }

        Rectangle {
            border.width: 1
            border.color: "black"

            anchors.fill: parent

            color: Qt.rgba(0, 0, 0, 0);
        }
    }


    DataTabWidget {

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: regionTree.right
        anchors.right: parent.right

        CavePage {
            property string label: "Overview"
            property string icon:  "icons/dataOverview.png"
        }

        SurveyEditor {
            property string label: "Data"
            property string icon: "icons/data.png"
        //    text: "This is the Data page"
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
            property string label: "Calibrations"
            property string icon: "icons/calibration.png"
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

}
