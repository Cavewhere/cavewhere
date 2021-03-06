/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import Cavewhere 1.0
import QtQuick.Controls 1.0 as Controls
import QtQuick.Layouts 1.1
import "Utils.js" as Utils

QQ.Item {
    id: clipArea

    property alias currentTrip: view.trip
    property Calibration currentCalibration: currentTrip.calibration === null ? defaultTripCalibartion : currentTrip.calibration
    readonly property alias contentWidth: scrollAreaId.width //For animation

    clip: false

    width: scrollAreaId.width
    height: 100

    TripLengthTask {
        id: tripLengthTask
        trip: currentTrip
    }

    Controls.ScrollView {
        id: scrollAreaId

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 1;

        width: flickableAreaId.contentWidth
        visible: true

        QQ.Flickable {
            id: flickableAreaId

            contentHeight: column.height
            contentWidth: Math.max(spaceAddBar.width + spaceAddBar.x, view.contentWidth + 2) + 20.0

            function ensureVisible(r){
                var contentY = flickableAreaId.contentY;
                if (flickableAreaId.contentY >= r.y) {
                    contentY = r.y;
                } else if (flickableAreaId.contentY+height <= r.y+r.height) {
                    contentY = r.y+r.height-height;
                }

                flickableAreaId.contentY = contentY;
            }

            QQ.Column {
                id: column

                spacing: 5

                ColumnLayout {
                    width: view.contentWidth

                    QQ.Item {
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
                            font.pixelSize: 20

                            onFinishedEditting: {
                                currentTrip.name = newText
                            }
                        }

                        QQ.Item {
                            Layout.fillWidth: true
                        }

                        DoubleClickTextInput {
                            id: tripDate
                            text: Qt.formatDate(currentTrip.date, "yyyy-MM-dd")

                            font.bold: true

                            onFinishedEditting: {
                                currentTrip.date = Date.fromLocaleDateString(Qt.locale(), newText, "yyyy-MM-dd");
                            }
                        }
                    }
                }

                BreakLine { }

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

                SurveyErrorOverview {
                    trip: currentTrip
                }

                SurveyChunkGroupView {
                    id: view

                    trip: defaultTrip

                    height: contentHeight
                    width: view.contentWidth

                    viewportX: flickableAreaId.contentX;
                    viewportY: flickableAreaId.contentY;
                    viewportWidth: scrollAreaId.viewport.width;
                    viewportHeight: scrollAreaId.viewport.height;

                    onEnsureVisibleRectChanged: flickableAreaId.ensureVisible(ensureVisibleRect);
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

                QQ.Image {
                    id: spaceAddBar
                    source: "qrc:icons/spacebar.png"

                    anchors.horizontalCenter: view.horizontalCenter

                    visible: currentTrip !== null && currentTrip.chunkCount > 0

                    Text {
                        anchors.centerIn: parent
                        text: "Press <b>Space</b> to add another data block";
                    }

                    QQ.MouseArea {
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
//                    visible: true
                    visible: currentTrip !== null && currentTrip.chunkCount === 0

                    onClicked: {
                        currentTrip.addNewChunk()
                    }
                }
            }
        }
    }

    QQ.MouseArea {
        anchors.fill: scrollAreaId
        onPressed: {
            scrollAreaId.forceActiveFocus()
            mouse.accepted = false;
        }
    }
}
