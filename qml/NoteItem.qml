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
    }

    NoteScaleInteraction {
        id: noteScaleInteraction
        anchors.fill: parent
        imageItem: noteArea
        basePanZoomInteraction: panZoomInteraction
        transformUpdater: transformUpdaterId
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [
            panZoomInteraction,
            addScrapInteraction,
            addStationInteraction,
            noteSelectionInteraction,
            noteNorthUpInteraction,
            noteScaleInteraction
        ]
        defaultInteraction: panZoomInteraction
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
        spacing: 5
        z: 2

        NoteResolution {
            id: noteResolutionId
            resolution: {
                if(note !== null) {
                    return note.imageResolution;
                }
                return null;
            }
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
            to: ""
            ScriptAction {
                script: {
                    //console.log("State change to default!");
                    scrapViewId.clearSelection();
                    interactionManagerId.active(panZoomInteraction)
                }
            }
        },

        Transition {
            to: "SELECT"
            ScriptAction {
                script: interactionManagerId.active(noteSelectionInteraction)
            }
        }

    ]

}
