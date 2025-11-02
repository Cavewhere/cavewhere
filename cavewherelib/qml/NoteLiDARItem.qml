import QtQuick
import cavewherelib

RegionViewer {
    id: rhiViewerId
    objectName: "rhiViewerId"

    property NoteLiDAR note

    scene: GltfScene {
        id: sceneId
        gltf.gltfFilePath: note ? RootData.project.absolutePath(note.filename) : ""
        gltf.futureManagerToken: RootData.futureManagerModel.token
        gltf.modelMatrix: {
            let mat = Qt.matrix4x4()
            if(note) {
                mat.rotate(note.noteTransformation.up)
            }
            return mat
        }

        gltf.onStatusChanged: () => {
                                  if(gltf.status == RenderGLTF.Ready) {
                                      turnTableInteractionId.zoomTo(gltf.boundingBox())
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

        BaseNoteLiDARStationInteraction {
            id: stationInteraction

        }

        TapHandler {
            id: addStationTapHandler
            enabled: false
            onSingleTapped: (eventPoint, button) =>
                            {
                                let rayHit = turnTableInteractionId.pick(eventPoint.position);
                                // console.log("RayHit:" + eventPoint.position + " " + rayHit.hit + " " + rayHit.pointModel)
                                if(rayHit.hit) {
                                    stationInteraction.addPoint(rayHit.pointModel, note);
                                }

                                // console.log("Single tap")
                            }
        }
    }

    NoteLiDARNorthInteraction {
        id: lidarNorthInteraction
        anchors.fill: parent
        viewer: rhiViewerId
        turnTableInteraction: turnTableInteractionId
        scene: sceneId
        note: rhiViewerId.note
    }

    NoteLiDARUpInteraction {
        id: lidarUpInteraction
        anchors.fill: parent
        viewer: rhiViewerId
        turnTableInteraction: turnTableInteractionId
        scene: sceneId
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
            turnTableInteractionId,
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
        selectionManager: SelectionManager {
            id: selectionManagerId
        }

        component: NoteLiDARStationItem {
            id: stationItemId

            objectName: "noteLiDARStation_" + pointIndex

            note: rhiViewerId.note
            parentView: noteStationRepeaterId
            onFinishedMoving:
                (position) => {
                    let rayHit = turnTableInteractionId.pick(position);
                    console.log("Finished moving:" + rayHit.hit + " " + rayHit.pointModel);
                    if(rayHit.hit) {
                        let index = note.index(stationItemId.pointIndex, 0)
                        note.setData(index, rayHit.pointModel, NoteLiDAR.PositionOnNoteRole);
                    }

                }
        }
    }

    NoteLiDARTransformEditor {
        noteTransform: note ? note.noteTransformation : null
        interactionManager: interactionManagerId
        northInteraction: lidarNorthInteraction
        upInteraction: lidarUpInteraction
        scaleInteraction: lidarScaleInteraction

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.topMargin: 5
    }

    states: [
        State {
            name: "SELECT"
            PropertyChanges {
                addStationTapHandler {
                    enabled: false
                }
            }
        },

        State {
            name: "ADD-STATION"
            PropertyChanges {
                addStationTapHandler {
                    enabled: true
                }
            }
        }
    ]
}
