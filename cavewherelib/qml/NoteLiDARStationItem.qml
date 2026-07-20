import QtQuick as QQ
import cavewherelib

PointItem {
    id: noteStationId

    required property NoteLiDAR note
    required property string name
    property QQ.vector3d position3D: Qt.vector3d(0, 0, 0)
    // Re-resolved when note rebinds; the validator below relaxes to
    // the external station grammar while the owning trip is attached.
    readonly property Trip parentTrip: note !== null ? note.parentTrip() : null

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

    StationValidator {
        id: stationValidatorId
        objectName: "stationValidator"
        // Same external-grammar relaxation as NoteStation (master plan
        // §7.2): attached trips reference upstream file station names.
        external: noteStationId.parentTrip !== null
                  && noteStationId.parentTrip.externalCenterline.entryFile.length > 0
    }

    StationDoubleClickTextInput {
        id: stationName

        anchors.verticalCenter: stationImage.verticalCenter
        anchors.left: stationImage.right
        pointItem: noteStationId
        validator: stationValidatorId
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
