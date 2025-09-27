/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
import QtQuick.Layouts

StandardPage {
    id: area

    objectName: "tripPage"

    property alias currentTrip: surveyEditor.currentTrip
    property string viewMode: ""
    property alias currentNoteIndex: notesGallery.currentNoteIndex

    function registerSubPages() {
        var oldCarpetPage = PageView.page.childPage("Carpet")
        if(oldCarpetPage !== RootData.pageSelectionModel.currentPage) {
            if(oldCarpetPage !== null) {
                RootData.pageSelectionModel.unregisterPage(oldCarpetPage)
            }

            if(PageView.page.name !== "Carpet") {
                var page = RootData.pageSelectionModel.registerSubPage(area.PageView.page,
                                                                       "Carpet",
                                                                       {"viewMode":"CARPET"});
            }
        }
    }

    /**
      Initilizing properties, if they aren't explicitly specified in the page
      */
    PageView.defaultProperties: {
        "currentTrip":null,
                "viewMode":""
    }

    PageView.defaultSelectionProperties: {
        "currentNoteIndex": 0
    }

    PageView.onPageChanged: registerSubPages()

    onViewModeChanged: {
        if(viewMode == "CARPET") {
            notesGallery.setMode("CARPET")
            state = "COLLAPSE" //Hide the survey data on the left side
        } else {
            notesGallery.setMode("DEFAULT")
            state = "" //Show the survey data on the left side
            surveyEditor.visible = true
            notesGallery.visible = true
        }
    }

    onCurrentNoteIndexChanged: {
        PageView.page.selectionProperties = { "currentNoteIndex":currentNoteIndex }
    }

    SurveyEditor {
        id: surveyEditor
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 5
        visible: true

        onCollapseClicked: {
            area.state = "COLLAPSE"
        }
    }

    QQ.Rectangle {
        id: collapseRectangleId
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        border.width: 1

        width: expandButton.width + 6
        visible: false

        ColumnLayout {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 3

            QC.RoundButton {
                id: expandButton
                icon.source: "qrc:/twbs-icons/icons/chevron-right.svg"
                implicitWidth: 20
                implicitHeight: 20
                onClicked: {
                    area.state = ""
                }
                radius: 0
            }

            QQ.Item {

                implicitWidth: tripNameVerticalText.implicitHeight
                implicitHeight: tripNameVerticalText.implicitWidth

                Text {
                    id: tripNameVerticalText
                    rotation: 270
                    text: area.currentTrip.name
                    x: -5
                    y: 20
                }
            }
        }

    }

    NotesGallery {
        id: notesGallery
        notesModel: SurveyNotesConcatModel {
            id: surveyNoteConcatModelId
            trip: area.currentTrip
            // area.currentTrip.notes
        }
        anchors.left: surveyEditor.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        clip: true

        onImagesAdded: (images) => {
                           console.log("Adding images:" + images)
            surveyNoteConcatModelId.addFiles(images)
        }

        // onBackClicked: {
        //     RootData.pageSelectionModel.back()
        // }

        // onModeChanged: {
        //     if(mode === "CARPET"
        //             && area.viewMode === "") {
        //         var page = area.PageView.page
        //         RootData.pageSelectionModel.gotoPageByName(area.PageView.page, "Carpet")
        //         RootData.pageSelectionModel.currentPage.selectionProperties = page.selectionProperties
        //     }
        // }
    }


//    QQ.Component.onCompleted: {
//        registerSubPages()
//    }

    states: [
        QQ.State {
            name: "COLLAPSE"

            QQ.PropertyChanges {
                collapseRectangleId {
                    visible: true
                }
            }

            QQ.AnchorChanges {
                target: notesGallery
                anchors.left: collapseRectangleId.right
            }

            QQ.PropertyChanges {
                surveyEditor {
                    visible: false
                }
            }
        }
    ]

    transitions: [
         QQ.Transition {
            from: ""
            to: "COLLAPSE"

            QQ.PropertyAction { target: surveyEditor; property: "visible"; value: true }
            QQ.PropertyAction { target: collapseRectangleId; property: "visible"; value: false }


            QQ.PropertyAction {
                target: surveyEditor; property: "clip"; value: true
            }

            QQ.ParallelAnimation {
                QQ.AnchorAnimation {
                    targets: [ notesGallery ]
                }

                QQ.NumberAnimation {
                    target: surveyEditor
                    property: "width"
                    to: 0
                }
            }

            QQ.PropertyAction {
                target: surveyEditor; property: "clip"; value: false
            }

            QQ.PropertyAction { target: collapseRectangleId; property: "visible"; value: true }
        },

         QQ.Transition {
            from: "COLLAPSE"
            to: ""


            QQ.PropertyAction { target: notesGallery; property: "anchors.left"; value: surveyEditor.right}

            QQ.ParallelAnimation {
                QQ.AnchorAnimation {
                    targets: [ notesGallery ]
                }

                QQ.NumberAnimation {
                    target: surveyEditor
                    property: "width"
                    to: surveyEditor.contentWidth
                }
            }
        }
    ]
}
