import QtQuick as QQ
import cavewherelib

BaseTurnTableInteraction {
    id: interactionId

    QQ.DragHandler {
        id: dragHandlerLeftId
        target: null
        acceptedButtons: Qt.LeftButton
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad

        onActiveChanged: {
            if(active) {
                interactionId.startPanning(centroid.position)
            }
        }

        onCentroidChanged: {
            if(active) {
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
                interactionId.startRotating(centroid.position)
            }
        }

        onCentroidChanged: {
            if(active) {
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
                interactionId.startRotating(centroid.position)
            }
        }

        onCentroidChanged: {
            if(active) {
                interactionId.rotate(centroid.position)
            }
        }
    }

    QQ.PinchHandler {
        id: pinchHandlerId
        target: null

        onActiveChanged: {
            if(active) {
                interactionId.startPanning(centroid.position)
            } else {
                pinchCooldownTimer.restart()
            }
        }

        onScaleChanged: (delta) => {
                          let zoomDelta = 1.0 - delta;
                          interactionId.zoom(centroid.position, zoomDelta)
                      }

        onTranslationChanged: (delta) => {
                                if(active) {
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
                interactionId.zoom(point.position, -deltaRotation)
                lastRotation = rotation
            }
        }
    }
}
