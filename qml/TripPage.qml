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

    SurveyEditor {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
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
