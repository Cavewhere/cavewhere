/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

CoordinatePicker {
    id: pickerId

    // The turn-table that owns canonical Pitch/Azimuth/CurrentRotation state.
    // We don't run a second BaseTurnTableInteraction inside the picker — that
    // would fork the camera-pose state. Instead we forward rotate + zoom slots
    // to this one, so the sidebar readouts and compass stay in sync.
    required property BaseTurnTableInteraction turnTableInteraction

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible

    onDeactivated: pickerId.clearPick()

    // Crosshair cursor while the interaction is active. acceptedButtons: NoButton
    // ensures clicks pass through to the TapHandler below.
    QQ.MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        cursorShape: Qt.CrossCursor
        hoverEnabled: true
    }

    QQ.TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: (eventPoint) => pickerId.pick(eventPoint.position)
    }

    TurnTableForwardingHandlers {
        turnTableInteraction: pickerId.turnTableInteraction
    }

    // Re-project the picked 3D scene point through the camera on every camera
    // change so the marker tracks the geometry as the user rotates/zooms.
    // The updater drives the anchor's x/y via the position3D property; the
    // visual rectangle is offset to center on that anchor.
    TransformUpdater {
        id: markerUpdaterId
        camera: pickerId.camera
        enabled: pickerId.hasPick
    }

    QQ.Item {
        id: markerVisibilityId
        visible: pickerId.hasPick
        z: 1

        QQ.Item {
            id: markerAnchorId
            property QQ.vector3d position3D: pickerId.scenePoint
            property bool inFrustum: true
            visible: markerAnchorId.inFrustum

            QQ.Rectangle {
                x: -width / 2
                y: -height / 2
                width: 12
                height: 12
                radius: width / 2
                color: Theme.accent
                border.color: Theme.text
                border.width: 1
            }

            QQ.Component.onCompleted: markerUpdaterId.addPointItem(markerAnchorId)
        }
    }

    QQ.Keys.onEscapePressed: pickerId.deactivate()
}
