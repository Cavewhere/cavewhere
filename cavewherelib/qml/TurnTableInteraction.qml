import QtQuick as QQ
import cavewherelib

BaseTurnTableInteraction {
    id: interactionId

    // Target whose worldRadius property the hold-P + wheel gesture writes to.
    // Externally bound so the interaction doesn't depend on scene structure.
    property LazLayersSceneNode pointCloudWorldRadiusTarget: null

    // Set true by GLTerrainRenderer while the P key is held. When true, the
    // wheel re-routes from camera zoom to point-cloud worldRadius tuning.
    property bool pKeyHeld: false

    // Multiplicative per-tick factor applied to worldRadius. The radius spans
    // [0.01, 50] m on the scene-node clamp, so a linear delta would feel
    // either jumpy at small values or sluggish at large ones; exp() keeps
    // each tick a fixed fraction of the current size. exp(deltaRotation *
    // 0.1) at a typical mouse tick (~12 deg * rotationScale 0.1 = 1.2 of
    // deltaRotation) gives ~13% per tick — ~6 ticks to double.
    readonly property real worldRadiusLogStep: 0.1

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
            if(deltaRotation === 0.0) {
                return
            }

            if(interactionId.pKeyHeld && interactionId.pointCloudWorldRadiusTarget) {
                // Wheel up (positive delta) grows points; wheel down shrinks.
                // Multiplicative — keeps the per-tick feel consistent across
                // the full clamped range.
                let target = interactionId.pointCloudWorldRadiusTarget
                target.worldRadius = target.worldRadius
                    * Math.exp(deltaRotation * interactionId.worldRadiusLogStep)
                lastRotation = rotation
                return
            }

            console.debug(interactLog, "wheel zoom", point.position, "delta=", -deltaRotation)
            interactionId.zoom(point.position, -deltaRotation)
            lastRotation = rotation
        }
    }
}
