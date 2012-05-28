import QtQuick 2.0
import Cavewhere 1.0

ImageItem {
    id: noteArea

    property Note note;

//    //Private properties
//    NoteTransform {
//        id: defaultNoteTransform
//    }

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
    }

    NoteStationInteraction {
        id: addStationInteraction
        anchors.fill: parent
        scrapView: scrapViewId
//        noteStationView: noteStationViewId
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

    NoteTransformEditor {
        id: noteTransformEditorId
        anchors.top: parent.top
        anchors.left: parent.left
        interactionManager: interactionManagerId
        northInteraction: noteNorthUpInteraction
        scaleInteraction: noteScaleInteraction
        scrap: {
            if(scrapViewId.selectedScrapItem !== null) {
                return scrapViewId.selectedScrapItem.scrap;
            }
            return null;
        }
        z: 2
    }

    //For rendering scraps onto the view
    ScrapView {
        id: scrapViewId
        note: noteArea.note
        transformUpdater: transformUpdaterId
    }


//    //For rendering note onto the note
//    NoteStationView {
//        id: noteStationViewId
//        note: noteArea.note
//        transformUpdater: transformUpdaterId
//        scrapView: scrapViewId
//        anchors.fill: parent
//    }

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
