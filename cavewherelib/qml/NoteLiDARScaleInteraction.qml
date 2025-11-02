/**************************************************************************
**
**    NoteLiDARScaleInteraction.qml
**    Placeholder interaction for drawing a LiDAR scale reference.
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

Interaction {
    id: lidarScaleInteraction
    objectName: "noteLiDARScaleInteraction"

    property NoteLiDARTransformation noteTransform
    property NoteLiDAR note
    property RegionViewer viewer
    property TurnTableInteraction turnTableInteraction
    property GltfScene scene

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible

    // TODO: Replace with full LiDAR scale measurement workflow.
    HelpBox {
        id: helpBox
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        text: "LiDAR scale tool coming soon."
        visible: lidarScaleInteraction.visible
    }

    QQ.Keys.onEscapePressed: lidarScaleInteraction.deactivate()
}

