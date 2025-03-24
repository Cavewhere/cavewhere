/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as Controls
import QtQuick.Layouts
import "Utils.js" as Utils

QQ.Item {
    id: clipArea

    property Trip currentTrip
    property TripCalibration currentCalibration: currentTrip.calibration
    readonly property alias contentWidth: scrollAreaId.width //For animation

    signal collapseClicked();

    clip: false

    width: scrollAreaId.width
    height: 100

    TripLengthTask {
        id: tripLengthTask
        trip: clipArea.currentTrip
    }

    SurveyEditorModel {
        id: editorModel
        trip: currentTrip
    }


    Controls.ScrollView {
        id: scrollAreaId

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 1;

        Controls.ScrollBar.vertical.stepSize: 50;

        width: 500; //flickableAreaId.contentWidth
        visible: true
        clip: true

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

        QQ.ListView {
            id: viewId
            objectName: "view"

            Controls.ButtonGroup {
                id: errorButtonGroupId
            }

            SurveyChunkTrimmer {
                id: surveyChunkTrimmerId
            }

            header: ColumnLayout {
                id: column
                spacing: 5
                width: scrollAreaId.width - 30


                ColumnLayout {
                    Layout.fillWidth: true


                    QQ.Item {
                        Layout.fillWidth: true
                        implicitHeight: collapseButton.height
                        SectionLabel {
                            text: "Trip"
                        }

                        Controls.Button {
                            id: collapseButton
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 3
                            icon.source: "qrc:/twbs-icons/icons/chevron-left.svg"
                            icon.width: 16
                            icon.height: 16
                            onClicked:  clipArea.collapseClicked()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        DoubleClickTextInput {
                            id: tripNameText
                            text: clipArea.currentTrip.name
                            font.bold: true
                            font.pixelSize: 20

                            onFinishedEditting: (newText) => {
                                                    clipArea.currentTrip.name = newText
                                                }
                        }

                        QQ.Item {
                            Layout.fillWidth: true
                        }

                        DoubleClickTextInput {
                            id: tripDate
                            objectName: "tripDate"

                            text: Qt.formatDate(clipArea.currentTrip.date, "yyyy-MM-dd")

                            font.bold: true

                            onFinishedEditting: function(newText) {
                                let dateTime = newText + " 00:00:00";
                                clipArea.currentTrip.date = Date.fromLocaleString(Qt.locale(), dateTime, "yyyy-MM-dd HH:mm:ss");
                            }
                        }
                    }
                }

                BreakLine { }

                CalibrationEditor {
                    Layout.fillWidth: true
                    calibration: currentTrip === null ? null : clipArea.currentTrip.calibration
                }

                BreakLine { }

                TeamTable {
                    id: teamTable
                    model: currentTrip !== null ? clipArea.currentTrip.team : null
                    Layout.fillWidth: true
                }

                BreakLine { }

                SectionLabel {
                    text: "Data"
                }

                SurveyErrorOverview {
                    trip: currentTrip
                }

                AddButton {
                    id: addSurveyData
                    objectName: "addSurveyData"
                    text: "Add Survey Data " + clipArea.currentTrip + " " + clipArea.currentTrip.chunkCount
                    Layout.alignment: Qt.AlignHCenter
                    // anchors.horizontalCenter: view.horizontalCenter
                    // visible: true
                    visible: clipArea.currentTrip !== null && clipArea.currentTrip.chunkCount === 0

                    onClicked: {
                        clipArea.currentTrip.addNewChunk()
                    }
                }
            }


            model: editorModel


            delegate: DrySurveyComponent {
                calibration: currentCalibration
                errorButtonGroup: errorButtonGroupId
                surveyChunkTrimmer: surveyChunkTrimmerId
                view: viewId
            }

            // section.property: "chunkId"
            // section.delegate: SurveyEditorColumnTitles {
            //     visible: {
            //         console.log("Section:" + section + " " + section.length);
            //         return section.length !== 0
            //     }

            //     onVisibleChanged: {
            //         console.log("Section visiblity" + visible)
            //         if(section.length === 0) {
            //             visible = false;
            //         }
            //     }
            // }

            footer: ColumnLayout {
                width: scrollAreaId.width - 30

                Text {
                    objectName: "totalLengthText"
                    // visible: !addSurveyData.visible
                    text: {
                        if(clipArea.currentTrip === null) { return "" }
                        var unit = ""
                        switch(clipArea.currentTrip.calibration.distanceUnit) {
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

                QQ.Image {
                    id: spaceAddBar
                    source: "qrc:icons/spacebar.png"

                    //                    anchors.horizontalCenter: view.horizontalCenter

                    visible: currentTrip !== null && currentTrip.numberOfChunks > 0
                    Layout.alignment: Qt.AlignHCenter

                    Text {
                        anchors.centerIn: parent
                        text: "Press <b>Space</b> to add another data block";
                    }

                    QQ.MouseArea {
                        anchors.fill: parent

                        onClicked: {
                            console.log("Mouse area click, new chunk!");
                            clipArea.currentTrip.addNewChunk();
                        }
                    }
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
