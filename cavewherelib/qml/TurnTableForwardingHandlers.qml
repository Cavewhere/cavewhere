/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// Right-drag rotate + wheel zoom passthrough to a BaseTurnTableInteraction.
//
// Used by interactions that take over left-click for their own purpose
// (CoordinatePickerInteraction, LazClipInteractionView) but still want the
// user to orient and zoom the camera with the same gestures the turn-table
// uses by default.
QQ.Item {
    id: rootId

    required property BaseTurnTableInteraction turnTableInteraction

    anchors.fill: parent

    QQ.DragHandler {
        target: null
        acceptedButtons: Qt.RightButton
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        onActiveChanged: {
            if (active) {
                rootId.turnTableInteraction.startRotating(centroid.position)
            }
        }
        onCentroidChanged: {
            if (active) {
                rootId.turnTableInteraction.rotate(centroid.position)
            }
        }
    }

    QQ.WheelHandler {
        property double lastRotation: 0.0
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        rotationScale: 0.1
        onRotationChanged: {
            const deltaRotation = rotationScale * (rotation - lastRotation)
            if (deltaRotation !== 0.0) {
                rootId.turnTableInteraction.zoom(point.position, -deltaRotation)
                lastRotation = rotation
            }
        }
    }
}
