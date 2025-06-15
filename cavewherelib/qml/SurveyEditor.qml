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

    property var window: QQ.Window.window

    clip: false

    width: scrollAreaId.width
    height: 100

    TripLengthTask {
        id: tripLengthTask
        trip: clipArea.currentTrip
    }


    SurveyEditorModel {
        id: editorModel
        trip: clipArea.currentTrip

        onLastChunkAdded: {
            editorFocusId.focusOnLastChunk()
        }
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

        QQ.ListView {
            id: viewId
            objectName: "view"

            Controls.ButtonGroup {
                id: errorButtonGroupId
            }

            SurveyChunkTrimmer {
                id: surveyChunkTrimmerId
            }

            StationValidator {
                id: stationValidatorId
            }

            DistanceValidator {
                id: distanceValidatorId
            }

            CompassValidator {
                id: compassValidatorId
            }

            ClinoValidator {
                id: clinoValidatorId
            }

            SurveyEditorFocus {
                id: editorFocusId
                trip: clipArea.currentTrip
                view: viewId
                model: editorModel

                onBoxIndexChanged: {
                    //Move the list view
                    let listViewIndex = editorModel.toModelRow(boxIndex.rowIndex)
                    if(boxIndex.rowType === SurveyEditorRowIndex.ShotRow) {
                        //Since the shot row has a height of zero, this -1 and +1 forces shot to be visible in the list view
                        view.positionViewAtIndex(listViewIndex - 1, QQ.ListView.Contain)
                        view.positionViewAtIndex(listViewIndex + 1, QQ.ListView.Contain)
                    } else {
                        view.positionViewAtIndex(listViewIndex, QQ.ListView.Contain)
                    }
                }
            }

            //reuseItem can cause instablity in testcases and crashes in qml, by default it's false
            // reuseItems: true

            currentIndex: -1 //The currentIndex should change

            //Prevents default keyboard interaction
            keyNavigationEnabled: false

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
                    text: "Add Survey Data "
                    Layout.alignment: Qt.AlignHCenter
                    visible: clipArea.currentTrip !== null && clipArea.currentTrip.chunkCount === 0

                    onClicked: {
                        clipArea.currentTrip.addNewChunk()
                    }
                }
            }


            model: editorModel

            SurveyEditorColumnTitles {
                id: titleTemplate
                visible: false
                listViewIndex: -1
                chunk: null
            }


            delegate: DrySurveyComponent {
                calibration: currentCalibration
                errorButtonGroup: errorButtonGroupId
                surveyChunkTrimmer: surveyChunkTrimmerId
                // view: viewId
                model: editorModel
                stationValidator: stationValidatorId
                distanceValidator: distanceValidatorId
                compassValidator: compassValidatorId
                clinoValidator: clinoValidatorId
                editorFocus: editorFocusId
                columnTemplate: titleTemplate
            }


            footer: ColumnLayout {
                width: scrollAreaId.width - 30

                Text {
                    objectName: "totalLengthText"
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
                    objectName: "spaceAddBar"
                    source: "qrc:icons/spacebar.png"

                    visible: currentTrip !== null && currentTrip.chunkCount > 0
                    Layout.alignment: Qt.AlignHCenter

                    Text {
                        anchors.centerIn: parent
                        text: "Press <b>Space</b> to add another data block";
                    }

                    QQ.MouseArea {
                        anchors.fill: parent

                        onClicked: {
                            clipArea.currentTrip.addNewChunk();
                        }
                    }
                }
            }
        }
    }
}

