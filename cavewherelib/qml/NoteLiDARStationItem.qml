import QtQuick as QQ
import cavewherelib

PointItem {
    id: noteStationId

    required property NoteLiDAR note
    required property string name
    property QQ.vector3d position3D: Qt.vector3d(0, 0, 0)

    width: 2
    height: 2

    signal finishedMoving(point position)

    // onPointIndexChanged: updateItem()

    function select() {
        noteStationId.selected = true
    }

    QQ.Keys.onDeletePressed: {
        note.removeStation(ponitIndex)
    }

    QQ.Keys.onPressed: (event) => {
                           if(event.key === Qt.Key_Backspace) {
                               note.removeStation(ponitIndex)
                           }
    }


    SelectedBackground {
        id: selectedBackground

        anchors.left: stationImage.left
        anchors.top: stationImage.top
        anchors.right: stationName.right
        anchors.bottom: stationName.bottom

        visible: noteStationId.selected //&& noteStationId.scrapItem.selected
    }

    StationImage {
        id: stationImage

        BasePointHandler {
            objectName: "stationIconHandler"
            anchors.fill: parent
            onPointSelected: noteStationId.select();
            parentView: noteStationId.parentView
            onPositionChanged: (point) => {
                                   //This updates the position but doesn't do the ray casting
                                   noteStationId.x = point.x
                                   noteStationId.y = point.y
                          }
            onFinishedMoving: (point) => {
                                  noteStationId.finishedMoving(point)
                              }

            onDoubleClicked: stationName.openEditor();
        }
    }

    StationDoubleClickTextInput {
        id: stationName

        anchors.verticalCenter: stationImage.verticalCenter
        anchors.left: stationImage.right
        pointItem: noteStationId
        text: name

        onFinishedEditting: (newText) => {
                                let index = note.index(noteStationId.pointIndex, 0)
                                note.setData(index, newText, NoteLiDAR.NameRole);


            // noteStationId.scrap.setStationData(Scrap.StationName, noteStationId.pointIndex, newText);
            // text = newText;
            // noteStationId.forceActiveFocus();
        }
    }
}
