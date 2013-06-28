import QtQuick 2.0
import Cavewhere 1.0
import QtQuick.Controls 1.0
import "Utils.js" as Utils

Rectangle {
    id: area

    anchors.fill: parent

    property alias currentTrip: view.trip
    property Calibration currentCalibration: currentTrip.calibration === null ? defaultTripCalibartion : currentTrip.calibration

    TripLengthTask {
        id: tripLengthTask
        trip: currentTrip
    }

    //    ScrollBar {
    //        id: surveyEditorScollbar
    //        anchors.top: flickArea.top
    //        anchors.bottom: flickArea.bottom
    //        anchors.left: flickArea.right
    //        orientation: Qt.Vertical
    //        minimumValue: 0
    //        maximumValue: flickArea.contentHeight - height < 0 ? 0 : flickArea.contentHeight - height
    //        visible: maximumValue > 0
    //        singleStep: 1
    //        onValueChanged: flickArea.contentY = value
    //    }


    //    Flickable {
    //        id: flickArea

    //        contentHeight: column.height
    //        width: Math.max(spaceAddBar.width + spaceAddBar.x, view.contentWidth + 2)

    //        anchors.top: parent.top
    //        anchors.bottom: parent.bottom
    //        anchors.left: parent.left
    //        anchors.margins: 1;

    //        clip: true;

    /**
              Moves the flickable such that r is always shown
              */


    //        onContentYChanged: {
    //            surveyEditorScollbar.value = contentY
    //        }

    ScrollView {
        id: scrollAreaId

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 1;

        width: Math.max(spaceAddBar.width + spaceAddBar.x, view.contentWidth + 2) + 20.0

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

//                viewportX: scrollAreaId.contentX;
//                viewportY: scrollAreaId.contentY;
                viewportWidth: scrollAreaId.width;
                viewportHeight: scrollAreaId.height;

                onEnsureVisibleRectChanged: scrollAreaId.ensureVisible(ensureVisibleRect);
            }

            Text {
                visible: !addSurveyData.visible
                text: {
                    if(currentTrip === null) { return "" }
                    var unit = ""
                    switch(currentTrip.calibration.distanceUnit) {
                    case Units.Meters:
                        unit = "m"
                        break;
                    case Units.Feet:
                        unit = "ft"
                        break;
                    }

                    return "Total Length: " + Utils.fixed(tripLengthTask.length, 2) + " " + unit;
                }
            }

            Image {
                id: spaceAddBar
                source: "qrc:icons/spacebar.png"

                anchors.horizontalCenter: view.horizontalCenter

                visible: currentTrip !== null && currentTrip.numberOfChunks > 0

                Text {
                    anchors.centerIn: parent
                    text: "Press <b>Space</b> to add another data block";
                }

                MouseArea {
                    anchors.fill: parent

                    onClicked: {
                        currentTrip.addNewChunk();
                    }
                }
            }

            AddButton {
                id: addSurveyData
                text: "Add Survey Data"
                anchors.horizontalCenter: view.horizontalCenter
                visible: currentTrip !== null && currentTrip.numberOfChunks === 0

                onClicked: {
                    currentTrip.addNewChunk()
                }
            }
        }
    }

    MouseArea {
        anchors.fill: scrollAreaId
        onPressed: {
            scrollAreaId.forceActiveFocus()
            mouse.accepted = false;
        }
    }

    NoteExplorer {
        noteModel: currentTrip !== null ? currentTrip.notes : null
        anchors.left: scrollAreaId.right
        anchors.right: parent.right
        anchors.top: area.top
        anchors.bottom: area.bottom
        clip: true
    }
}

