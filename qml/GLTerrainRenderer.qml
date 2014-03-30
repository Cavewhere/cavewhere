/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0

RegionViewer {
    id: renderer
    MouseArea {
        id: interaction
        anchors.fill: parent;
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: {
            if(mouse.button == Qt.LeftButton) {
                renderer.state = "panState"
                startPanning(Qt.point(mouse.x, mouse.y));
            } else {
                renderer.state = "rotateState"
                startRotating(Qt.point(mouse.x, mouse.y));
            }
        }

        onWheel: {
            zoom(Qt.point(wheel.x, wheel.y), -wheel.angleDelta.y)
        }

        onPositionChanged: {}
    }

//    ChoosePaperSizeInteraction {
//        viewer: renderer
//        z: 1
//    }

    LinePlotLabelView {
        id: labelView
        anchors.fill: parent
        camera: renderer.camera
        region: rootData.region
    }

    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.right: parent.right
        anchors.rightMargin: 20
        spacing: 10

        ScaleBar {
            id: scaleBar
            visible: renderer.orthoProjection.enabled
            camera: renderer.camera
            anchors.bottom: compassItemId.bottom
            maxTotalWidth: renderer.width * 0.50
            minTotalWidth: renderer.height * 0.2
        }

        CompassItem {
            id: compassItemId
            width: 175
            height: width
            camera: renderer.camera
            rotation: renderer.rotation
            shaderDebugger: renderer.scene.shaderDebugger
            antialiasing: false
        }
    }

    states: [
        State {
            name: "panState"
            PropertyChanges {
                target: interaction;

                onPositionChanged: {
                    renderer.pan(Qt.point(mouseX, mouseY))
                }

                onReleased: {
                    renderer.state = ""; //Go back to orginial state
                }
            }
        },

        State {
            name: "rotateState"
            PropertyChanges {
                target: interaction;

                onPositionChanged: {
                    renderer.rotate(Qt.point(mouseX, mouseY));
                }

                onReleased: {
                    renderer.state = "";
                }
            }
        }

    ]
}

