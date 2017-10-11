/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0
import QtQuick.Controls 1.0 as Controls
import QtQuick.Layouts 1.1
import "Utils.js" as Utils

Item {
    id: clipArea

    //    property alias currentTrip: view.trip
    property Trip currentTrip
    property Calibration currentCalibration: currentTrip.calibration === null ? defaultTripCalibartion : currentTrip.calibration
    readonly property alias contentWidth: scrollAreaId.width //For animation

    clip: false

    width: scrollAreaId.width
    height: 100

    TripLengthTask {
        id: tripLengthTask
        trip: currentTrip
    }

    SurvexEditorModel {
        id: editorModel
        trip: currentTrip
    }

    Controls.ScrollView {
        id: scrollAreaId

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 1;

        width: 500; //flickableAreaId.contentWidth
        visible: true

        //        Flickable {
        //            id: flickableAreaId

        //            contentHeight: column.height
        //            contentWidth: Math.max(spaceAddBar.width + spaceAddBar.x, view.contentWidth + 2) + 20.0

        //            function ensureVisible(r){
        //                var contentY = flickableAreaId.contentY;
        //                if (flickableAreaId.contentY >= r.y) {
        //                    contentY = r.y;
        //                } else if (flickableAreaId.contentY+height <= r.y+r.height) {
        //                    contentY = r.y+r.height-height;
        //                }

        //                flickableAreaId.contentY = contentY;
        //            }

        ListView {
            id: view

            header: ColumnLayout {
                id: column
                spacing: 5
                width: scrollAreaId.width - 30


                ColumnLayout {
                    Layout.fillWidth: true

                   Item {
                        Layout.fillWidth: true
                        implicitHeight: collapseButton.height
                        SectionLabel {
                            text: "Trip"
                        }

                        Button {
                            id: collapseButton
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 3
                            iconSource: "qrc:/icons/moreArrowLeft.png"
                            onClicked: {
                                area.state = "COLLAPSE"
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        DoubleClickTextInput {
                            id: tripNameText
                            text: currentTrip.name
                            font.bold: true
                            font.pointSize: 20

                            onFinishedEditting: {
                                currentTrip.name = newText
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        DoubleClickTextInput {
                            id: tripDate
                            text: Qt.formatDateTime(currentTrip.date, "yyyy-MM-dd")
                            font.bold: true

                            onFinishedEditting: {
                                currentTrip.date = newText
                            }
                        }
                    }
                }

                BreakLine { }

                CalibrationEditor {
                    Layout.fillWidth: true
                    calibration: currentTrip === null ? null : currentTrip.calibration
                }

                BreakLine { }

                TeamTable {
                    id: teamTable
                    model: currentTrip !== null ? currentTrip.team : null
                    Layout.fillWidth: true
                }

                BreakLine { }

                SectionLabel {
                    text: "Data"
                }

//                SurveyErrorOverview {
//                    trip: currentTrip
//                }
            }


            model: editorModel

            delegate: DrySurveyComponent {
                calibration: currentCalibration
            }

            section.property: "chunkId"
            section.delegate: SurveyEditorColumnTitles {
                visible: {
                    console.log("Section:" + section + " " + section.length);
                    return section.length !== 0
                }

                onVisibleChanged: {
                    console.log("Section visiblity" + visible)
                    if(section.length === 0) {
                        visible = false;
                    }
                }
            }

            footer: ColumnLayout {
                width: scrollAreaId.width - 30

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

                    //                    anchors.horizontalCenter: view.horizontalCenter

                    visible: currentTrip !== null && currentTrip.numberOfChunks > 0
                    Layout.alignment: Qt.AlignHCenter

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
}

//MouseArea {
//    anchors.fill: scrollAreaId
//    onPressed: {
//        scrollAreaId.forceActiveFocus()
//        mouse.accepted = false;
//    }
//}
//}
