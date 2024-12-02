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

    property alias currentTrip: view.trip
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
                            text: Qt.formatDate(clipArea.currentTrip.date, "yyyy-MM-dd")

                            font.bold: true

                            onFinishedEditting: function(newText) {
                                clipArea.currentTrip.date = Date.fromLocaleDateString(Qt.locale(), newText, "yyyy-MM-dd");
                            }
                        }
                    }
                }

                BreakLine { }

                CalibrationEditor {
                    width: view.contentWidth
                    calibration: clipArea.currentTrip === null ? null : clipArea.currentTrip.calibration
                }

                BreakLine { }

                TeamTable {
                    id: teamTable
                    model: clipArea.currentTrip !== null ? clipArea.currentTrip.team : null
                    width: view.contentWidth
                }

                BreakLine { }

                SectionLabel {
                    text: "Data"
                }

                SurveyErrorOverview {
                    trip: clipArea.currentTrip
                }

                SurveyChunkGroupView {
                    id: view

                    trip: RootData.defaultTrip

                    height: contentHeight
                    width: view.contentWidth

                    viewportX: flickableAreaId.contentX;
                    viewportY: flickableAreaId.contentY;
                    // viewportWidth: scrollAreaId.viewport.width;
                    // viewportHeight: scrollAreaId.viewport.height;
                    viewportWidth: flickableAreaId.visibleArea.widthRatio * flickableAreaId.width
                    viewportHeight: flickableAreaId.visibleArea.heightRatio * flickableAreaId.height

                    onEnsureVisibleRectChanged: flickableAreaId.ensureVisible(ensureVisibleRect);
                }

                Text {
                    visible: !addSurveyData.visible
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

                    anchors.horizontalCenter: view.horizontalCenter

                    visible: clipArea.currentTrip !== null && clipArea.currentTrip.chunkCount > 0

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

                AddButton {
                    id: addSurveyData
                    text: "Add Survey Data"
                    anchors.horizontalCenter: view.horizontalCenter
//                    visible: true
                    visible: clipArea.currentTrip !== null && clipArea.currentTrip.chunkCount === 0

                    onClicked: {
                        clipArea.currentTrip.addNewChunk()
                    }
                }
            }
        }
    }

    QQ.MouseArea {
        anchors.fill: scrollAreaId
        onPressed: function(mouse) {
            scrollAreaId.forceActiveFocus()
            mouse.accepted = false;
        }
    }
}
