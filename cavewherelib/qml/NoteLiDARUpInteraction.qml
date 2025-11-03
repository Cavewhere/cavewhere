/**************************************************************************
**
**    NoteLiDARUpInteraction.qml
**    Placeholder interaction for setting the LiDAR up direction.
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

Interaction {
    id: lidarUpInteraction
    objectName: "noteLiDARUpInteraction"

    property NoteLiDARTransformation noteTransform
    property NoteLiDAR note
    property RegionViewer viewer
    property TurnTableInteraction turnTableInteraction
    property GltfScene scene

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible

    // TODO: Replace with LiDAR up-direction handling workflow.
    HelpBox {
        id: helpBox
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        text: "LiDAR up-direction tool coming soon."
        visible: lidarUpInteraction.visible
    }

    QQ.Keys.onEscapePressed: lidarUpInteraction.deactivate()
}

