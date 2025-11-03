/**************************************************************************
**
**    NoteLiDARNorthInteraction.qml
**    Placeholder interaction for orienting LiDAR notes. Functionality will
**    be implemented in a future change.
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

Interaction {
    id: lidarNorthInteraction
    objectName: "noteLiDARNorthInteraction"

    property NoteLiDARTransformation noteTransform
    property NoteLiDAR note
    property RegionViewer viewer
    property TurnTableInteraction turnTableInteraction
    property GltfScene scene

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible

    // TODO: Replace with full LiDAR north/up alignment workflow.
    HelpBox {
        id: helpBox
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        text: "LiDAR orientation tool coming soon."
        visible: lidarNorthInteraction.visible
    }

    QQ.Keys.onEscapePressed: lidarNorthInteraction.deactivate()
}

