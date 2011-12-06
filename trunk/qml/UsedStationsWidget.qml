import QtQuick 1.1
import Cavewhere 1.0

Rectangle {
    id: usedStationsArea
    radius: 4
    gradient: Gradient {
        GradientStop {
            position: 0
            color: "#cccccc"
        }

        GradientStop {
            position: 0.18
            color: "#ffffff"
        }
    }
    // transformOrigin: Item.Center
    //    anchors.bottomMargin: 5
    //    anchors.left: parent.left
    //    anchors.leftMargin: 5
    //    anchors.top: parent.top
    //    anchors.bottom: parent.bottom
    //    anchors.topMargin: 5
    // border.color: "#000000"
    border.width: 1

    ListView {
        id: usedStationsView

        anchors.rightMargin: 2
        anchors.bottomMargin: 2
        anchors.leftMargin: 2
        anchors.top: lineId.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.topMargin: 0
        clip: true
        model: usedStationsModel.usedStations;

        delegate: Text {
            id: textId

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 3
            anchors.rightMargin: 3

            text: modelData


        }


        UsedStationTaskManager {
            id: usedStationsModel;
            cave: currentCave != null ? currentCave : null
            listenToCaveChanges: parent.visible
        }
    }

    Rectangle {
        id: lineId
        height: 1
        color: Qt.rgba(0.0, 0.0, 0.0, 1.0);
        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.left: parent.left
        anchors.leftMargin: 2
        anchors.top: usedStationsLabel.bottom
        //anchors.bottom: usedStationsView.top

    }

    Text {
        id: usedStationsLabel
        text: "Used Stations"
        font.bold: true
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 4
        font.pixelSize:14
    }


}
