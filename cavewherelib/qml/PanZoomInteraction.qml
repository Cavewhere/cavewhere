/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

Interaction {
    id: interactionId
    required property ImageBackground target
    property alias dragAcceptedButtons: dragHandlerId.acceptedButtons

    //We need to make this false, so default state disables the interaction
    enabled: false

    function refit() {
        target.x = target.parent.width * 0.5 - target.image.sourceSize.width * 0.5;
        target.y = target.parent.height * 0.5 - target.image.sourceSize.height * 0.5;

        let imageAspect = target.width / target.height
        let scaleX = 0.0
        let scaleY = 0.0
        if(imageAspect >= 1.0) {
            scaleX = target.parent.width / target.image.sourceSize.width;
            scaleY = target.parent.height / target.image.sourceSize.height;
        } else {
            //Different aspect ratios, flip the scales
            scaleX = target.parent.width / target.image.sourceSize.height;
            scaleY = target.parent.height / target.image.sourceSize.width;
        }

        // Choose the smaller scale factor to ensure the image fits within the item
        target.scale = Math.min(scaleX, scaleY);
    }

    function zoomAround(factor, scenePoint) {
        if (factor > 0 && isFinite(factor)) {
            const local = target.mapFromItem(null, scenePoint)
            const before = target.mapToItem(target.parent, local)
            target.scale *= factor
            const after = target.mapToItem(target.parent, local)
            target.x += before.x - after.x
            target.y += before.y - after.y
        }
    }

    QQ.Connections {
        target: interactionId.target.image
        function onStatusChanged() {
            if(interactionId.target.image.status === QQ.Image.Ready) {
                interactionId.state = "AUTO_FIT"
                interactionId.refit()
            }
        }
    }

    QQ.WheelHandler {
        id: wheelHandlerId
        enabled: interactionId.enabled
        target: interactionId.target
        parent: interactionId.target
        property: "scale"
        targetScaleMultiplier: 1.1
        targetTransformAroundCursor: true
    }

    QQ.DragHandler {
        id: dragHandlerId
        enabled: interactionId.enabled
        target: interactionId.target
        parent: interactionId.target
        onActiveChanged: {
            //Disable auto centering
            interactionId.state = ""
        }
    }

    //Trackpad pinches are applied as relative steps on the target's live
    //transform. Driving the target from PinchHandler jumps (#747): it scales
    //from its own persistentScale, stale after any other zoom, and a click
    //mid-gesture deactivates it, so later steps restart from position (0,0).
    QQ.PinchHandler {
        id: trackpadPinchHandlerId
        enabled: interactionId.enabled
        target: null
        parent: interactionId.target
        acceptedDevices: QQ.PointerDevice.TouchPad
        rotationAxis.enabled: false
        onScaleChanged: (delta) => interactionId.zoomAround(delta, trackpadPinchHandlerId.centroid.scenePosition)
    }

    //Touchscreens keep PinchHandler's own transform, which also pans with the fingers
    QQ.PinchHandler {
        id: touchPinchHandlerId
        enabled: interactionId.enabled
        target: interactionId.target
        parent: interactionId.target
        acceptedDevices: QQ.PointerDevice.TouchScreen
        rotationAxis.minimum: interactionId.target.rotation
        rotationAxis.maximum: interactionId.target.rotation
        persistentRotation: interactionId.target.rotation
    }

    //Every other zoom (wheel, trackpad, refit, another tool's instance) moves
    //the scale behind the touch pinch's back; resync its basis
    QQ.Connections {
        target: interactionId.target
        function onScaleChanged() {
            if (!touchPinchHandlerId.active) {
                touchPinchHandlerId.persistentScale = interactionId.target.scale
            }
        }
    }

    states: [
        QQ.State {
            name: "AUTO_FIT"
            QQ.PropertyChanges {
                target: interactionId.target ? interactionId.target.parent : null
                onWidthChanged: interactionId.refit()
                onHeightChanged: interactionId.refit()
            }
        }
    ]
}
