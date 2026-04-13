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
    property int currentNoteIndex: notesGalleryLoader.item ? notesGalleryLoader.item.currentNoteIndex : 0

    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse

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
        if(area.isNarrow) return;
        let ng = notesGalleryLoader.item
        if(!ng) return;
        if(viewMode == "CARPET") {
            ng.setMode("CARPET")
            state = "COLLAPSE" //Hide the survey data on the left side
        } else {
            ng.setMode("DEFAULT")
            state = "" //Show the survey data on the left side
            surveyEditor.visible = true
            ng.visible = true
        }
    }

    onIsNarrowChanged: {
        if(isNarrow && state === "COLLAPSE") {
            state = ""
        }
    }

    onCurrentNoteIndexChanged: {
        PageView.page.selectionProperties = { "currentNoteIndex":currentNoteIndex }
    }

    SurveyNotesConcatModel {
        id: surveyNoteConcatModelId
        trip: area.currentTrip
    }

    SurveyEditor {
        id: surveyEditor
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 5
        visible: true
        isNarrow: area.isNarrow
        notesModel: surveyNoteConcatModelId

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
        color: Theme.background

        ColumnLayout {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 3

            RoundButton {
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

                QC.Label {
                    id: tripNameVerticalText
                    rotation: 270
                    text: area.currentTrip.name
                    anchors.centerIn: parent
                    transformOrigin: QQ.Item.Center
                }
            }
        }

    }

    QQ.Loader {
        id: notesGalleryLoader
        active: !area.isNarrow
        visible: !area.isNarrow
        anchors.left: surveyEditor.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        clip: true

        sourceComponent: QQ.Component {
            NotesGallery {
                notesModel: surveyNoteConcatModelId
                onImagesAdded: (images) => {
                    surveyNoteConcatModelId.addFiles(images)
                }
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
                collapseRectangleId {
                    visible: true
                }
            }

            QQ.AnchorChanges {
                target: notesGalleryLoader
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
                    targets: [ notesGalleryLoader ]
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


            QQ.PropertyAction { target: notesGalleryLoader; property: "anchors.left"; value: surveyEditor.right}

            QQ.ParallelAnimation {
                QQ.AnchorAnimation {
                    targets: [ notesGalleryLoader ]
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
