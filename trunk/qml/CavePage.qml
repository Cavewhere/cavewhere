import Qt 4.7
import Cavewhere 1.0

Rectangle {
    id: cavePageArea

    Text {
        id: caveNameText
        text: caveData.name //From c++
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 5
        font.bold: true
        font.pointSize: 20
    }

    Rectangle {
        id: usedStationsArea
        width: 219
        height: 455
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
        transformOrigin: Item.Center
        anchors.bottomMargin: 5
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.top: caveNameText.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: 5
        border.color: "#000000"

        ListView {
            id: usedStationsView

            x: 0
            y: 58
            width: 373
            height: 267
            anchors.rightMargin: 2
            anchors.bottomMargin: 2
            anchors.leftMargin: 2
            anchors.top: line.bottom
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.topMargin: 0
            clip: true
            model: usedStationsModel.usedStations;

            delegate: Rectangle {
                height: line.height
                width: parent.width
                color: Qt.rgba(0.0, 0.0, 0.0, 0.0);

                Text {
                    id: line
                    text: modelData

                }
            }

            UsedStationTaskManager {
                id: usedStationsModel;
                cave: caveData
                listenToCaveChanges: parent.visible
            }
        }

        Rectangle {
            id: line
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



    width: 500
    height: 500
}
