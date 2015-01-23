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

Rectangle {
    id: area

    property alias currentTrip: view.trip
    property string viewMode: ""

    property Calibration currentCalibration: currentTrip.calibration === null ? defaultTripCalibartion : currentTrip.calibration

    /**
      Initilizing properties, if they aren't explicitly specified in the page
      */
    PageView.defaultProperties: {
        "currentTrip":null,
                "viewMode":""
    }

    onViewModeChanged: {
        if(viewMode == "CARPET") {
            notesGallery.setMode("CARPET")
            state = "COLLAPSE" //Hide the survey data on the left side
        } else {
            notesGallery.setMode("DEFAULT")
            state = "" //Show the survey data on the left side
        }
    }

    TripLengthTask {
        id: tripLengthTask
        trip: currentTrip
    }

    Item {
        id: clipArea
        clip: false

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left

        width: scrollAreaId.width

        Controls.ScrollView {
            id: scrollAreaId

            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.margins: 1;

            width: flickableAreaId.contentWidth
            visible: true

            Flickable {
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

                Column {
                    id: column

                    spacing: 5

                    ColumnLayout {
                        width: view.contentWidth

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
                                iconSource: "qrc:/icons/moreArrow.png"
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
        }

        MouseArea {
            anchors.fill: scrollAreaId
            onPressed: {
                scrollAreaId.forceActiveFocus()
                mouse.accepted = false;
            }
        }
    }


    Rectangle {
        id: collapseRectangleId
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        border.width: 1

        width: collapseButton.width + 6
        visible: false

        ColumnLayout {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 3

            Button {
                id: expandButton
                iconSource: "qrc:/icons/moreArrow.png"
                onClicked: {
                    area.state = ""
                }
            }

            Item {

                implicitWidth: tripNameVerticalText.implicitHeight
                implicitHeight: tripNameVerticalText.implicitWidth

                Text {
                    id: tripNameVerticalText
                    rotation: 270
                    text: currentTrip.name
                    x: -5
                    y: 10
                }
            }
        }

    }

    SurveyEditorNoteGallery {
        id: notesGallery
        editor: area
        notesModel: currentTrip.notes
        anchors.left: clipArea.right
        anchors.right: parent.right
        anchors.top: area.top
        anchors.bottom: area.bottom
        clip: true

        onImagesAdded: {
            currentTrip.notes.addFromFiles(images, rootData.project)
        }

        onBackClicked: {
            rootData.pageSelectionModel.back()
        }

        onModeChanged: {
            console.log("Mode changed:" + mode + (mode === "CARPET"))
            if(mode === "CARPET" && area.viewMode === "") {
                console.log("I get here!")
                rootData.pageSelectionModel.gotoPageByName(area.PageView.page, "Carpet")
            }
        }

    }

    Component.onCompleted: {
        var page = rootData.pageSelectionModel.registerSubPage(area.PageView.page,
                                                               "Carpet",
                                                               {"viewMode":"CARPET"});
        console.log("Registering Carpet page:" + page)
    }

    states: [
        State {
            name: "COLLAPSE"

            PropertyChanges {
                target: collapseRectangleId
                visible: true
            }

            AnchorChanges {
                target: notesGallery
                anchors.left: collapseRectangleId.right
            }

            PropertyChanges {
                target: clipArea
                visible: false
            }
        }
    ]

    transitions: [
        Transition {
            from: ""
            to: "COLLAPSE"

            PropertyAction { target: clipArea; property: "visible"; value: true }


            PropertyAction {
                target: clipArea; property: "clip"; value: true
            }

            ParallelAnimation {
                AnchorAnimation {
                    targets: [ notesGallery ]
                }

                NumberAnimation {
                    target: clipArea
                    property: "width"
                    to: 0
                }
            }

            PropertyAction {
                target: clipArea; property: "clip"; value: false
            }

        },

        Transition {
            from: "COLLAPSE"
            to: ""

            PropertyAction { target: notesGallery; property: "anchors.left"; value: clipArea.right}

            ParallelAnimation {
                AnchorAnimation {
                    targets: [ notesGallery ]
                }

                NumberAnimation {
                    target: clipArea
                    property: "width"
                    to: scrollAreaId.width
                }
            }

        }
    ]

}
