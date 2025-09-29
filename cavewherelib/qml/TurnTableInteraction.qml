import QtQuick as QQ
import cavewherelib

BaseTurnTableInteraction {
    id: interactionId

    QQ.DragHandler {
        id: dragHandlerLeftId
        target: null
        acceptedButtons: Qt.LeftButton

        onActiveTranslationChanged: {
            console.log("Drag left:" + activeTranslation + persistentTranslation)
            interactionId.pan(Qt.point(persistentTranslation.x, persistentTranslation.y))
        }

        onGrabChanged: (transition, point) => {
                           if(transition === QQ.PointerDevice.GrabExclusive) {
                               console.log("Start panning:" + point.pressPosition);
                               persistentTranslation = point.pressPosition
                               interactionId.startPanning(point.pressPosition);
                           }
                       }
    }

    QQ.DragHandler {
        id: dragHandlerRightId
        target: null
        acceptedButtons: Qt.RightButton

        onActiveTranslationChanged: {
            console.log("Drag right:" + activeTranslation)
            interactionId.rotate(Qt.point(persistentTranslation.x, persistentTranslation.y));
        }

        onGrabChanged: (transition, point) => {
                           if(transition === QQ.PointerDevice.GrabExclusive) {
                               console.log("Start panning:" + point.pressPosition);
                               persistentTranslation = point.pressPosition
                               interactionId.startRotating(point.pressPosition);
                           }
                       }

    }

    QQ.TapHandler {
        onSingleTapped: {
            console.log("Single tap")
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
        acceptedDevices: QQ.PointerDevice.Mouse //| QQ.PointerDevice.TouchPad
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
