import QtQuick as QQ
import cavewherelib

BaseTurnTableInteraction {
    id: interactionId

    QQ.WheelHandler {
        id: wheelHandlerId
        target: interactionId
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        property double lastRotation: 0.0
        rotationScale: 0.1
        onRotationChanged: {
            let deltaRotation = rotationScale * (rotation - lastRotation);
            if(deltaRotation !== 0.0) {
                interactionId.zoom(point.position, -deltaRotation)
                lastRotation = rotation
            }
        }
    }

    // QQ.PinchHandler {
    //     target: null
    //     rotationAxis.enabled: false
    //     xAxis.enabled: false
    //     yAxis.enabled: false
    //     scaleAxis.onActiveValueChanged: (delta) => {
    //                                         let zoomDelta = 1.0 - delta;
    //                                         interactionId.zoom(centroid.position, zoomDelta)
    //                                     }
    // }

    QQ.MouseArea {
        id: mouseArea
        // enabled: multiTouchArea.subMouseEnabled
        anchors.fill: parent;
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        onPressed: function(mouse) {
            if(mouse.button === Qt.LeftButton) {
                state = "panState"
                interactionId.startPanning(Qt.point(mouse.x, mouse.y));
            } else {
                state = "rotateState"
                interactionId.startRotating(Qt.point(mouse.x, mouse.y));
            }
        }


        states: [
            QQ.State {
                name: "panState"
                QQ.PropertyChanges {
                    mouseArea {
                        onPositionChanged: {
                            if(mouseArea.pressed) {
                                interactionId.pan(Qt.point(mouseX, mouseY))
                            }
                        }

                        onReleased: {
                            interactionId.state = ""; //Go back to orginial state
                            //                                }
                        }
                    }
                }
            },

            QQ.State {
                name: "rotateState"
                QQ.PropertyChanges {
                    mouseArea {
                        onPositionChanged: {
                            if(mouseArea.pressed) {
                                interactionId.rotate(Qt.point(mouseX, mouseY));
                            }
                        }

                        onReleased: {
                            interactionId.state = "";
                        }
                    }
                }
            }

        ]
    }
}
