import QtQuick
import cavewherelib

RegionViewer {
    id: rhiViewerId
    objectName: "rhiViewerId"

    property NoteLiDAR note
    property bool isNarrow: false

    //Set by NotesGallery's CARPET state. Named the same on NoteItem so the
    //gallery sets one property on every note editor it drives. Stations are
    //only shown while editing, so they can't be accidentally selected, moved,
    //or deleted when just viewing the note.
    property bool editingOverlaysVisible: false

    //The gallery's tool state, pulled rather than pushed. This editor's tools are
    //driven by PropertyChanges, which revert on any mode it declares no State for,
    //so an unhandled mode leaves every tool off without needing a teardown
    //transition (NoteItem drives its tools from transitions and does need one).
    property string toolMode: NoteToolMode.none

    //Icon capture overrides the tool state without assigning to state, which
    //would destroy the binding below and strand the viewer on the last mode.
    property bool capturingIcon: false

    function captureIconIfNeeded() {
        if (!note || !RootData.noteLiDARManager) {
            return
        }

        if (note.iconImagePath && note.iconImagePath.length > 0) {
            return
        }

        if (sceneId.gltf.status !== RenderGLTF.Ready) {
            return
        }

        const targetNote = note

        rhiViewerId.capturingIcon = true

        //grabToImage returns false without ever calling back when the viewer has
        //no window or no size, which gltf Ready can beat. Unhandled, capturingIcon
        //latches on and pins state to CAPTURE_ICON for the life of the item.
        let grabStarted = rhiViewerId.grabToImage(function(result) {
            rhiViewerId.capturingIcon = false
            if (!targetNote || !result || !result.image) {
                return
            }
            RootData.noteLiDARManager.saveIcon(result.image, targetNote)
        })

        if (!grabStarted) {
            rhiViewerId.capturingIcon = false
        }
    }

    scene: GltfScene {
        id: sceneId
        gltf.gltfFilePath: note ? RootData.project.absolutePath(note) : ""
        gltf.futureManagerToken: RootData.futureManagerModel.token
        gltf.modelMatrix: {
            let mat = Qt.matrix4x4()
            if(note) {
                mat.rotate(note.noteTransformation.up)
            }
            return mat
        }

        gltf.onStatusChanged: () => {
                                  if(gltf.status === RenderGLTF.Ready) {
                                      turnTableInteractionId.zoomTo(gltf.boundingBox())
                                      captureIconIfNeeded()
                                  }
                              }
    }

    orthoProjection.enabled: true
    perspectiveProjection.enabled: false

    state: capturingIcon ? NoteToolMode.captureIcon : toolMode

    TurnTableInteraction {
        id: turnTableInteractionId
        objectName: "turnTableInteraction"
        anchors.fill: parent
        camera: rhiViewerId.camera
        scene: rhiViewerId.scene
        // gridPlane: RootData.regionSceneManager.gridPlane.plane
    }

    NoteLiDARAddStationInteraction {
        id: lidarAddStationInteraction
        anchors.fill: parent
        note: rhiViewerId.note
        turnTableInteraction: turnTableInteractionId
    }

    NoteLiDARNorthInteraction {
        id: lidarNorthInteraction
        anchors.fill: parent
        modelMatrix: sceneId.gltf.modelMatrix
        turnTableInteraction: turnTableInteractionId
        note: rhiViewerId.note
    }

    NoteLiDARUpInteraction {
        id: lidarUpInteraction
        anchors.fill: parent
        modelMatrix: sceneId.gltf.modelMatrix
        turnTableInteraction: turnTableInteractionId
        note: rhiViewerId.note
    }

    NoteLiDARScaleInteraction {
        id: lidarScaleInteraction
        anchors.fill: parent
        viewer: rhiViewerId
        turnTableInteraction: turnTableInteractionId
        scene: sceneId
        note: rhiViewerId.note
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [
            // turnTableInteractionId, //Don't add, adding this turns off turnTableInteraction for other tools
            lidarAddStationInteraction,
            lidarNorthInteraction,
            lidarUpInteraction,
            lidarScaleInteraction
        ]
        defaultInteraction: turnTableInteractionId
    }

    //Stations are added to this container so hiding them is one property, and
    //doesn't fight the transform updater's per-station viewport clipping.
    //Mirrors NoteItem's scrapPointsContainer.
    Item {
        id: stationContainerId
        anchors.fill: parent
        visible: rhiViewerId.editingOverlaysVisible
    }

    Item3DRepeater {
        id: noteStationRepeaterId
        anchors.fill: parent

        targetItem: stationContainerId
        camera: rhiViewerId.camera
        model: note
        positionRole: NoteLiDAR.UpPositionRole
        selectionManager: SelectionManager {
            id: selectionManagerId
        }

        component: NoteLiDARStationItem {
            id: stationItemId

            objectName: "noteLiDARStation_" + pointIndex

            enabled: turnTableInteractionId.enabled
            note: rhiViewerId.note
            parentView: noteStationRepeaterId
            onFinishedMoving:
                (position) => {
                    let rayHit = turnTableInteractionId.pick(position);
                    // console.log("Finished moving:" + rayHit.hit + " " + rayHit.pointModel);
                    if(rayHit.hit) {
                        let index = note.index(stationItemId.pointIndex, 0)
                        note.setData(index, rayHit.pointModel, NoteLiDAR.PositionOnNoteRole);
                    }
                }
        }
    }


    NoteLiDARTransformEditor {
        id: transformEditorId
        collapsed: rhiViewerId.isNarrow
        note: rhiViewerId.note
        interactionManager: interactionManagerId
        northInteraction: lidarNorthInteraction
        upInteraction: lidarUpInteraction
        scaleInteraction: lidarScaleInteraction
        visible: true

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.topMargin: 5
    }

    //No State for NoteToolMode.none: QQuickStateGroup treats the empty name as
    //the base state and never looks up a State object by it, so such a State is
    //never applied and extending it inherits nothing. Base-state values belong
    //inline on the item instead (see transformEditorId.visible above).
    states: [
        State {
            name: NoteToolMode.select
            PropertyChanges { target: lidarAddStationInteraction; enabled: false; visible: false }
        },

        State {
            name: NoteToolMode.addStation
            PropertyChanges { target: lidarAddStationInteraction; enabled: true; visible: true }
        },

        State {
            name: NoteToolMode.captureIcon
            //The icon is a picture of the model alone, so keep the editing
            //overlays out of the grab even when captured from carpet mode.
            PropertyChanges {
                transformEditorId.visible: false
                stationContainerId.visible: false
            }
        }
    ]


}
