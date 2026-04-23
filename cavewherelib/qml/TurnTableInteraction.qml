import QtQuick as QQ
import cavewherelib

BaseTurnTableInteraction {
    id: interactionId

    QQ.DragHandler {
        id: dragHandlerLeftId
        target: null
        acceptedButtons: Qt.LeftButton

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

    QQ.DragHandler {
        id: dragHandlerRightId
        target: null
        acceptedButtons: Qt.RightButton

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
        target: null

        onScaleChanged: (delta) => {
                          let zoomDelta = 1.0 - delta;
                          interactionId.zoom(centroid.position, zoomDelta)
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
