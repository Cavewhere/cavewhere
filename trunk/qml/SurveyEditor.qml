import Qt 4.7
import Cavewhere 1.0

Rectangle {
    id: area

    anchors.fill: parent

    property alias currentTrip: view.trip
    property Calibration currentCalibration: currentTrip.calibration === null ? defaultTripCalibartion : currentTrip.calibration

    Flickable {
        id: flickArea

        contentHeight: column.height
        width: Math.max(spaceAddBar.width + spaceAddBar.x, view.contentWidth)

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 1;

        clip: true;

        /**
              Moves the flickable such that r is always shown
              */
        function ensureVisible(r){
            if (contentY >= r.y) {
                contentY = r.y;
            } else if (contentY+height <= r.y+r.height) {
                contentY = r.y+r.height-height;
            }
        }
        Column {
            id: column

            spacing: 5

            CalibrationEditor {
                width: view.contentWidth
                calibration: currentTrip === null ? null : currentTrip.calibration
            }

            BreakLine { }

            TeamTable {
                id: teamTable
                model: currentTrip !== null ? currentTrip.team : null
                width: view.contentWidth
            }

            BreakLine { }

            SectionLabel {
                text: "Data"
            }

            SurveyChunkGroupView {
                id: view

                height: contentHeight
                width: view.contentWidth

                viewportX: flickArea.contentX;
                viewportY: flickArea.contentY;
                viewportWidth: flickArea.width;
                viewportHeight: flickArea.height;

                onEnsureVisibleRectChanged: flickArea.ensureVisible(ensureVisibleRect);
            }

            Image {
                id: spaceAddBar
                source: "qrc:icons/spacebar.png"

         //       y: view.y + view.contentHeight
                anchors.horizontalCenter: view.horizontalCenter

                Text {
                    anchors.centerIn: parent
                    text: {
                        if(currentTrip) {
                            if(currentTrip.numberOfChunks === 0) {
                                return "Press <b>Space</b> to start";
                            } else {
                                return "Press <b>Space</b> to add another data block";
                            }
                        } else {
                            return "Error: Current trip is null";
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent

                    onClicked: {
                        currentTrip.addNewChunk();
                    }
                }
            }
        }
    }

    NoteExplorer {
        noteModel: currentTrip !== null ? currentTrip.notes : null
        anchors.left: flickArea.right
        anchors.right: parent.right
        anchors.top: area.top
        anchors.bottom: area.bottom
        clip: true
    }

//    Keys.onSpacePressed: {
//        //Add chunk
//        currentTrip.addNewChunk();
//    }

//    onVisibleChanged: {
//        focus = visible
//    }
}

