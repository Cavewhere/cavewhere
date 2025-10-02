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
        gltf.modelMatrix: note ? note.modelMatrix : Qt.matrix4x4()
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
                                console.log("RayHit:" + eventPoint.position + " " + rayHit.hit + " " + rayHit.pointModel)
                                if(rayHit.hit) {
                                    stationInteraction.addPoint(rayHit.pointModel, note);
                                }

                                console.log("Single tap")
                            }
        }
    }

    Item3DRepeater {
        anchors.fill: parent
        camera: rhiViewerId.camera
        model: note
        positionRole: NoteLiDAR.ScenePositionRole
        component: Item {
            property vector3d position3D: Qt.vector3d(0, 0, 0)
            Rectangle {
                width: 5
                height: 5
                color: "red"
                anchors.centerIn: parent
            }
        }
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
