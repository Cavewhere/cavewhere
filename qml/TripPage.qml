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

StandardPage {
    id: area

    property alias currentTrip: surveyEditor.currentTrip
    property string viewMode: ""
    property alias currentNoteIndex: notesGallery.currentNoteIndex

    function registerSubPages() {
        if(currentTrip) {
            var oldCarpetPage = PageView.page.childPage("Carpet")
            if(oldCarpetPage !== rootData.pageSelectionModel.currentPage) {
                if(oldCarpetPage !== null) {
                    rootData.pageSelectionModel.unregisterPage(oldCarpetPage)
                }

                if(PageView.page.name !== "Carpet") {
                    var page = rootData.pageSelectionModel.registerSubPage(area.PageView.page,
                                                                           "Carpet",
                                                                           {"viewMode":"CARPET"});
                }
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
        visible: true
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

            Button {
                id: expandButton
                iconSource: "qrc:/icons/moreArrow.png"
                onClicked: {
                    area.state = ""
                }
            }

            QQ.Item {

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

    NotesGallery {
        id: notesGallery
        notesModel: currentTrip.notes
        anchors.left: surveyEditor.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        clip: true

        onImagesAdded: {
            currentTrip.notes.addFromFiles(images, rootData.project)
        }

        onBackClicked: {
            rootData.pageSelectionModel.back()
        }

        onModeChanged: {
            if(mode === "CARPET" && area.viewMode === "") {
                var page = area.PageView.page
                rootData.pageSelectionModel.gotoPageByName(area.PageView.page, "Carpet")
                rootData.pageSelectionModel.currentPage.selectionProperties = page.selectionProperties
            }
        }
    }



//    QQ.Component.onCompleted: {
//        registerSubPages()
//    }

    states: [
        QQ.State {
            name: "COLLAPSE"

            QQ.PropertyChanges {
                target: collapseRectangleId
                visible: true
            }

            QQ.AnchorChanges {
                target: notesGallery
                anchors.left: collapseRectangleId.right
            }

            QQ.PropertyChanges {
                target: surveyEditor
                visible: false
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
