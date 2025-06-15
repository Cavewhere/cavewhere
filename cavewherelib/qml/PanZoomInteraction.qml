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
    required property QQ.Image target
    property alias dragAcceptedButtons: dragHandlerId.acceptedButtons

    //We need to make this false, so default state disables the interaction
    enabled: false

    function refit() {
        target.x = target.parent.width * 0.5 - target.sourceSize.width * 0.5;
        target.y = target.parent.height * 0.5 - target.sourceSize.height * 0.5;

        let imageAspect = target.width / target.height
        let scaleX = 0.0
        let scaleY = 0.0
        if(imageAspect >= 1.0) {
            scaleX = target.parent.width / target.sourceSize.width;
            scaleY = target.parent.height / target.sourceSize.height;
        } else {
            //Different aspect ratios, flip the scales
            scaleX = target.parent.width / target.sourceSize.height;
            scaleY = target.parent.height / target.sourceSize.width;
        }

        // Choose the smaller scale factor to ensure the image fits within the item
        target.scale = Math.min(scaleX, scaleY);
        pinchHandlerId.persistentScale = target.scale
    }

    QQ.Connections {
        target: interactionId.target
        function onStatusChanged() {
            if(interactionId.target.status == QQ.Image.Ready) {
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

    QQ.PinchHandler {
        id: pinchHandlerId
        enabled: interactionId.enabled
        target: interactionId.target
        parent: interactionId.target
        rotationAxis.minimum: interactionId.target.rotation
        rotationAxis.maximum: interactionId.target.rotation
        persistentRotation: interactionId.target.rotation
    }

    states: [
        QQ.State {
            name: "AUTO_FIT"
            QQ.PropertyChanges {
                interactionId.target.parent.onWidthChanged: interactionId.refit()
                interactionId.target.parent.onHeightChanged: interactionId.refit()
            }
        }
    ]
}
