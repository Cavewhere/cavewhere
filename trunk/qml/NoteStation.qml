import QtQuick 1.0
import Cavewhere 1.0

Item {
    id: noteStation

    property variant noteViewer;
    property variant note;
    property variant stationId;
    property alias selected: selectedBackground.visible;

    //    x: -width / 2;
    //    y: -height / 2;

    width: 2
    height: 2
    //  color: "red"

    //    Image {
    //        id: selectedBackground
    //        anchors.centerIn: parent
    //        source: "qrc:icons/stationSelected.png"

    //        visible: false
    //    }

    Rectangle {
        id: selectedBackground

        anchors.left: stationImage.left
        anchors.top: stationImage.top
        anchors.right: stationName.right
        anchors.bottom: stationName.bottom
        anchors.margins: -2

        radius: 4

        color: "#418CFF"
        border.width: 1
        border.color: "white"

        opacity: .5

        visible: false
    }

    Image {
        id: stationImage
        anchors.centerIn: parent
        source: "qrc:icons/stationGood.png"

        width: sourceSize.width
        height: sourceSize.height


        MouseArea {
            id: stationMouseArea
            anchors.fill: parent

            property variant lastPoint;
            property bool ignoreLength;

            onClicked: {
                noteStation.selected = true
            }

            onReleased: {

            }

            onPressed: {
                lastPoint = Qt.point(mouse.x, mouse.y);
                ignoreLength = false;
            }

            onMousePositionChanged: {
                //Make sure the mouse has move at least three pixel from where it's started
                var length = Math.sqrt(Math.pow(lastPoint.x - mouse.x, 2) + Math.pow(lastPoint.y - mouse.y, 2));
                if(length > 3 || ignoreLength) {
                    ignoreLength = true
                    var parentCoord = mapToItem(noteViewer, mouse.x, mouse.y);
                    noteViewer.moveStation(Qt.point(parentCoord.x, parentCoord.y), note, stationId)
                }
            }
        }
    }

    DoubleClickTextInput {
        id: stationName

        style: Text.Outline
        styleColor: "#FFFFFF"
        font.bold: true

        //So we don't add new station when we click on the station
        acceptMousePress: true

        anchors.verticalCenter: stationImage.verticalCenter
        anchors.left: stationImage.right

        onFinishedEditting: {
            note.setStationData(Note.StationName, stationId, newText);
            text = newText;
        }
    }


    onNoteChanged: {
        console.debug("Note station:" + note.stationData(Note.StationName, stationId));
        stationName.text = note.stationData(Note.StationName, stationId)
    }



}
