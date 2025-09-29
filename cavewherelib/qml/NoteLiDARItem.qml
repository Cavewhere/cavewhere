import QtQuick
import cavewherelib

RegionViewer {
    id: rhiViewerId
    objectName: "rhiViewerId"

    property NoteLiDAR note

    scene: GltfScene {
        gltf.gltfFilePath: note ? RootData.project.absolutePath(note.filename) : ""
        gltf.futureManagerToken: RootData.futureManagerModel.token
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
