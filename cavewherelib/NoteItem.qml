/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

ImageItem {
    id: noteArea

    property Note note;
    property bool scrapsVisible: false
    // property bool scrapsVisible: false

    imageRotation: note ? note.rotate : 0
    source: note ? RootData.cavewhereImageUrl(note.original) : ""

    clip: true

    PanZoomInteraction {
        id: panZoomInteraction
        // anchors.fill: parent
        // camera: cameraId
        // enabled: false
        target: noteArea.targetItem
        // persistentScale:
    }

    // ScrapInteraction {
    //     id: addScrapInteraction
    //     anchors.fill: parent
    //     note: noteArea.note
    //     basePanZoomInteraction: panZoomInteraction
    //     noteCamera: cameraId
    //     // imageItem: noteArea
    //     outlinePointView: {
    //         if(scrapViewId.selectedScrapItem !== null) {
    //             return scrapViewId.selectedScrapItem.outlinePointView
    //         }
    //         return null;
    //     }
    //     scrap: {
    //         if(scrapViewId.selectedScrapItem !== null) {
    //             return scrapViewId.selectedScrapItem.scrap
    //         }
    //         return null
    //     }
    // }

    // NoteStationInteraction {
    //     id: addStationInteraction
    //     anchors.fill: parent
    //     scrapView: scrapViewId
    //     basePanZoomInteraction: panZoomInteraction
    //     imageItem: noteArea
    // }

    // NoteLeadInteraction {
    //     id: addLeadInteraction
    //     anchors.fill: parent
    //     scrapView: scrapViewId
    //     basePanZoomInteraction: panZoomInteraction
    //     imageItem: noteArea
    // }

    NoteItemSelectionInteraction {
        id: noteSelectionInteraction
        // anchors.fill: parent
        target: noteArea.targetItem
        scrapView: scrapViewId
        imageItem: noteArea
        // basePanZoomInteraction: panZoomInteraction
        // onActivated: {
        //     console.log("Note selection interaction activated!")
        // }
    }

    NoteNorthInteraction {
        id: noteNorthUpInteraction

        // anchors.fill: parent
        // camera: cameraId
        // basePanZoomInteraction: panZoomInteraction
        // transformUpdater: transformUpdaterId
        z:1
    }

    NoteScaleInteraction {
        id: noteScaleInteraction
        target: noteArea.targetItem
        // anchors.fill: parent
        imageItem: noteArea
        // basePanZoomInteraction: panZoomInteraction
        // transformUpdater: transformUpdaterId
        note: noteArea.note
        // z:1
    }

    NoteDPIInteraction {
        id: noteDPIInteraction
        target: noteArea.targetItem
        anchors.fill: parent
        imageItem: noteArea
        zoom: noteArea.targetItem.scale
        // basePanZoomInteraction: panZoomInteraction
        // transformUpdater: transformUpdaterId
        imageResolution: noteArea.note != null ? noteArea.note.imageResolution : null
        z:1
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [
            panZoomInteraction,
            // addScrapInteraction,
            // addStationInteraction,
            // addLeadInteraction,
            noteSelectionInteraction,
            // noteNorthUpInteraction,
            // noteScaleInteraction,
            noteDPIInteraction
        ]
        defaultInteraction: panZoomInteraction
        // defaultInteraction: noteSelectionInteraction

        onActiveInteractionChanged: {
            if(activeInteraction == defaultInteraction) {
                switch(noteArea.state) {
                case "ADD-SCRAP":
                    // addScrapInteraction.activate();
                    break;
                case "ADD-STATION":
                    // addStationInteraction.activate();
                    break;
                case "ADD-LEAD":
                    // addLeadInteraction.activate();
                    break;
                case "SELECT":
                    noteSelectionInteraction.activate();
                    break;
                }
            }
        }

    }

    // component PositionRectangle : Positioner {
    //     id: posId
    //     parent: noteArea.targetItem
    //     QQ.Rectangle {
    //         color: "green"
    //         width: 5
    //         height: width
    //         anchors.centerIn: parent

    //         onScaleChanged: {
    //             console.log("Scale changed:" + scale)
    //         }

    //         Text {
    //             text: "(" + posId.x + ", " + posId.y + ")"
    //         }
    //     }
    // }

    // PositionRectangle {
    //     id: rectId0
    //     parent: noteArea.targetItem

    // }
    // PositionRectangle {
    //     id: rectId1
    //     x: 500
    //     y: 500
    //     parent: noteArea.targetItem
    // }

    // PositionRectangle {
    //     id: rectId2
    //     x: 1000
    //     y: 500
    //     parent: noteArea.targetItem
    // }

    ColumnLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.topMargin: 5
        spacing: 5
        z: 2

        NoteResolution {
            id: noteResolutionId
            note: noteArea.note
            onActivateDPIInteraction: interactionManagerId.active(noteDPIInteraction)
            visible: noteArea.scrapsVisible
        }

        // NoteTransformEditor {
        //     id: noteTransformEditorId
        //     interactionManager: interactionManagerId
        //     northInteraction: noteNorthUpInteraction
        //     scaleInteraction: noteScaleInteraction
        //     scrap: {
        //         if(scrapViewId.selectedScrapItem !== null) {
        //             return scrapViewId.selectedScrapItem.scrap;
        //         }
        //         return null;
        //     }
        // }

        // LeadInfoEditor {
        //     id: leadInfoEditor
        //     leadView: scrapViewId.selectedScrapItem !== null ? scrapViewId.selectedScrapItem.leadView : null
        // }
    }

    //For rendering scraps onto the view
    ScrapView {
        id: scrapViewId
        parent: noteArea.targetItem
        note: noteArea.note

        zoom: noteArea.targetItem.scale //Zoom is needed for keep the line width constant
        visible: noteArea.scrapsVisible
    }

    states: [
        QQ.State {
            name: "ADD-STATION"
        },

        QQ.State {
            name: "ADD-SCRAP"
        },

        QQ.State {
            name: "ADD-LEAD"
        },

        QQ.State {
            name: "SELECT"
        }
    ]

    transitions: [
        //  QQ.Transition {
        //     to: "ADD-SCRAP"
        //     QQ.ScriptAction {
        //         script: {
        //             interactionManagerId.active(addScrapInteraction)
        //             addScrapInteraction.startNewScrap()
        //         }
        //     }
        // },

        //  QQ.Transition {
        //     to: "ADD-STATION"
        //     QQ.ScriptAction {
        //         script: interactionManagerId.active(addStationInteraction)
        //     }
        // },

        //  QQ.Transition {
        //     to: "ADD-LEAD"
        //     QQ.ScriptAction {
        //         script: interactionManagerId.active(addLeadInteraction)
        //     }
        // },

         QQ.Transition {
            to: ""
            QQ.ScriptAction {
                script: {
                    scrapViewId.clearSelection();
                    interactionManagerId.active(panZoomInteraction)
                }
            }
        },

         QQ.Transition {
            to: "SELECT"
            QQ.ScriptAction {
                script: {
                    interactionManagerId.active(noteSelectionInteraction)
                }
            }
        }

    ]

}
