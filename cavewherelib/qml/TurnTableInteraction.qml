import QtQuick as QQ
import cavewherelib

BaseTurnTableInteraction {
    id: interactionId

    QQ.LoggingCategory {
        id: interactLog
        name: "cw.interaction.qml"
        defaultLogLevel: QQ.LoggingCategory.Warning
    }

    QQ.DragHandler {
        id: dragHandlerLeftId
        target: null
        acceptedButtons: Qt.LeftButton
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad

        onActiveChanged: {
            if(active) {
                console.debug(interactLog, "left-drag startPanning", centroid.position)
                interactionId.startPanning(centroid.position)
            }
        }

        onCentroidChanged: {
            if(active) {
                console.debug(interactLog, "left-drag pan", centroid.position)
                interactionId.pan(centroid.position)
            }
        }
    }

    QQ.Timer {
        id: pinchCooldownTimer
        interval: 250
    }

    QQ.DragHandler {
        id: dragHandlerTouchId
        target: null
        acceptedDevices: QQ.PointerDevice.TouchScreen
        enabled: !pinchHandlerId.active && !pinchCooldownTimer.running

        onActiveChanged: {
            if(active) {
                console.debug(interactLog, "touch-drag startRotating", centroid.position)
                interactionId.startRotating(centroid.position)
            }
        }

        onCentroidChanged: {
            if(active) {
                console.debug(interactLog, "touch-drag rotate", centroid.position)
                interactionId.rotate(centroid.position)
            }
        }
    }

    QQ.DragHandler {
        id: dragHandlerRightId
        target: null
        acceptedButtons: Qt.RightButton
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad

        onActiveChanged: {
            if(active) {
                console.debug(interactLog, "right-drag startRotating", centroid.position)
                interactionId.startRotating(centroid.position)
            }
        }

        onCentroidChanged: {
            if(active) {
                console.debug(interactLog, "right-drag rotate", centroid.position)
                interactionId.rotate(centroid.position)
            }
        }
    }

    QQ.PinchHandler {
        id: pinchHandlerId
        target: null

        onActiveChanged: {
            if(active) {
                console.debug(interactLog, "pinch startPanning", centroid.position)
                interactionId.startPanning(centroid.position)
            } else {
                pinchCooldownTimer.restart()
            }
        }

        onScaleChanged: (delta) => {
                          let zoomDelta = 1.0 - delta;
                          console.debug(interactLog, "pinch zoom", centroid.position, "delta=", zoomDelta)
                          interactionId.zoom(centroid.position, zoomDelta)
                      }

        onTranslationChanged: (delta) => {
                                if(active) {
                                    console.debug(interactLog, "pinch pan", centroid.position)
                                    interactionId.pan(centroid.position)
                                }
                            }
    }


    QQ.WheelHandler {
        id: wheelHandlerId
        target: interactionId
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        property double lastRotation: 0.0
        rotationScale: 0.1
        onRotationChanged: {
            let deltaRotation = rotationScale * (rotation - lastRotation);
            if(deltaRotation !== 0.0) {
                console.debug(interactLog, "wheel zoom", point.position, "delta=", -deltaRotation)
                interactionId.zoom(point.position, -deltaRotation)
                lastRotation = rotation
            }
        }
    }
}
