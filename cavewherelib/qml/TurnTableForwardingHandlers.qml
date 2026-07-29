/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// Left-drag pan + right-drag rotate + wheel zoom passthrough to a
// BaseTurnTableInteraction.
//
// Used by interactions that take over left-*click* for their own purpose
// (MeasurementInteractionView, CoordinatePickerInteraction, LazClipInteractionView)
// but still want the user to move, orient, and zoom the camera with the same
// gestures the turn-table uses by default. A left click still reaches the host's
// TapHandler: it keeps only a passive grab, so the pan DragHandler takes over
// once the pointer passes the drag threshold, and a plain click still fires.
QQ.Item {
    id: rootId

    required property BaseTurnTableInteraction turnTableInteraction
    property bool rotationEnabled: true
    property bool panEnabled: true

    anchors.fill: parent

    QQ.DragHandler {
        target: null
        enabled: rootId.panEnabled
        acceptedButtons: Qt.LeftButton
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        onActiveChanged: {
            if (active) {
                rootId.turnTableInteraction.startPanning(centroid.position)
            }
        }
        onCentroidChanged: {
            if (active) {
                rootId.turnTableInteraction.pan(centroid.position)
            }
        }
    }

    QQ.DragHandler {
        target: null
        enabled: rootId.rotationEnabled
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
