import QtQuick
import cavewherelib

RegionViewer {
    id: rhiViewerId
    objectName: "rhiViewerId"

    property NoteLiDAR note

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
        const previousState = state

        state = "CAPTURE_ICON"

        rhiViewerId.grabToImage(function(result) {
            rhiViewerId.state = previousState
            if (!targetNote || !result || !result.image) {
                return
            }
            RootData.noteLiDARManager.saveIcon(result.image, targetNote)
        })
    }

    scene: GltfScene {
        id: sceneId
        gltf.gltfFilePath: {
            console.log("LiDAR note path:" + RootData.project.absolutePath(note))
            return note ? RootData.project.absolutePath(note) : ""
        }
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

    state: "SELECT"

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

    Item3DRepeater {
        id: noteStationRepeaterId
        anchors.fill: parent
        camera: rhiViewerId.camera
        model: note
        positionRole: NoteLiDAR.UpPositionRole
        visible: true
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

    states: [
        State {
            name: "BASE"
            PropertyChanges {
                transformEditorId.visible: true
                noteStationRepeaterId.visible: true
            }
        },

        State {
            name: "SELECT"
            extend: "BASE"
            PropertyChanges { target: lidarAddStationInteraction; enabled: false; visible: false }
        },

        State {
            name: "ADD-STATION"
            extend: "BASE"
            PropertyChanges { target: lidarAddStationInteraction; enabled: true; visible: true }
        },

        State {
            name: "CAPTURE_ICON"
            PropertyChanges {
                transformEditorId.visible: false
                noteStationRepeaterId.visible: false
            }
        }
    ]


}
