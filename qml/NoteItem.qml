/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0

ImageItem {
    id: noteArea

    property Note note;
    property alias scrapsVisible: scrapViewId.visible

    projectFilename: project.filename

    clip: true
    rotation: note !== null ? note.rotate : 0

    PanZoomInteraction {
        id: panZoomInteraction
        anchors.fill: parent
        camera: noteArea.camera
    }

    ScrapInteraction {
        id: addScrapInteraction
        anchors.fill: parent
        note: noteArea.note
        basePanZoomInteraction: panZoomInteraction
        imageItem: noteArea
        outlinePointView: {
            if(scrapViewId.selectedScrapItem !== null) {
                return scrapViewId.selectedScrapItem.outlinePointView
            }
            return null;
        }
        scrap: {
            if(scrapViewId.selectedScrapItem !== null) {
                return scrapViewId.selectedScrapItem.scrap
            }
            return null
        }
    }

    NoteStationInteraction {
        id: addStationInteraction
        anchors.fill: parent
        scrapView: scrapViewId
        basePanZoomInteraction: panZoomInteraction
        imageItem: noteArea
    }

    NoteLeadInteraction {
        id: addLeadInteraction
        anchors.fill: parent
        scrapView: scrapViewId
        basePanZoomInteraction: panZoomInteraction
        imageItem: noteArea
    }

    NoteItemSelectionInteraction {
        id: noteSelectionInteraction
        anchors.fill: parent
        scrapView: scrapViewId
        imageItem: noteArea
        basePanZoomInteraction: panZoomInteraction
    }

    NoteNorthInteraction {
        id: noteNorthUpInteraction
        anchors.fill: parent
        imageItem: noteArea
        basePanZoomInteraction: panZoomInteraction
        transformUpdater: transformUpdaterId
        z:1
    }

    NoteScaleInteraction {
        id: noteScaleInteraction
        anchors.fill: parent
        imageItem: noteArea
        basePanZoomInteraction: panZoomInteraction
        transformUpdater: transformUpdaterId
        note: noteArea.note
        z:1
    }

    NoteDPIInteraction {
        id: noteDPIInteraction
        anchors.fill: parent
        imageItem: noteArea
        basePanZoomInteraction: panZoomInteraction
        transformUpdater: transformUpdaterId
        imageResolution: note != null ? note.imageResolution : null
        z:1
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [
            panZoomInteraction,
            addScrapInteraction,
            addStationInteraction,
            addLeadInteraction,
            noteSelectionInteraction,
            noteNorthUpInteraction,
            noteScaleInteraction,
            noteDPIInteraction
        ]
        defaultInteraction: panZoomInteraction

        onActiveInteractionChanged: {
            if(activeInteraction == defaultInteraction) {
                switch(noteArea.state) {
                case "ADD-SCRAP":
                    addScrapInteraction.activate();
                    break;
                case "ADD-STATION":
                    addStationInteraction.activate();
                    break;
                case "ADD-LEAD":
                    addLeadInteraction.activate();
                    break;
                case "SELECT":
                    noteSelectionInteraction.activate();
                    break;
                }
            }
        }

    }

    //This allows note coordinates to be mapped to opengl coordinates
    TransformUpdater {
        id: transformUpdaterId
        camera: noteArea.camera
        modelMatrix: noteArea.modelMatrix
    }

    Column {
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
            visible: scrapsVisible
        }

        NoteTransformEditor {
            id: noteTransformEditorId
            interactionManager: interactionManagerId
            northInteraction: noteNorthUpInteraction
            scaleInteraction: noteScaleInteraction
            scrap: {
                if(scrapViewId.selectedScrapItem !== null) {
                    return scrapViewId.selectedScrapItem.scrap;
                }
                return null;
            }
        }

        LeadInfoEditor {
            id: leadInfoEditor
            leadView: scrapViewId.selectedScrapItem !== null ? scrapViewId.selectedScrapItem.leadView : null
        }
    }

    //For rendering scraps onto the view
    ScrapView {
        id: scrapViewId
        note: noteArea.note
        transformUpdater: transformUpdaterId
    }

    states: [
        State {
            name: "ADD-STATION"
        },

        State {
            name: "ADD-SCRAP"
        },

        State {
            name: "ADD-LEAD"
        },

        State {
            name: "SELECT"
        }
    ]

    transitions: [
        Transition {
            to: "ADD-SCRAP"
            ScriptAction {
                script: {
                    interactionManagerId.active(addScrapInteraction)
                    addScrapInteraction.startNewScrap()
                }
            }
        },

        Transition {
            to: "ADD-STATION"
            ScriptAction {
                script: interactionManagerId.active(addStationInteraction)
            }
        },

        Transition {
            to: "ADD-LEAD"
            ScriptAction {
                script: interactionManagerId.active(addLeadInteraction)
            }
        },

        Transition {
            to: ""
            ScriptAction {
                script: {
                    scrapViewId.clearSelection();
                    interactionManagerId.active(panZoomInteraction)
                }
            }
        },

        Transition {
            to: "SELECT"
            ScriptAction {
                script: {
                    interactionManagerId.active(noteSelectionInteraction)
                }
            }
        }

    ]

}
