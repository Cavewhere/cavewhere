/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0

RegionViewer {
    id: renderer

    property alias turnTableInteraction: turnTableInteractionId
    property alias interactionManager: interactionManagerId
    property alias leadView: leadViewId

    clip: true



    TurnTableInteraction {
        id: turnTableInteractionId
        anchors.fill: parent
        camera: renderer.camera
        scene: renderer.scene
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [
            turnTableInteractionId
        ]
        defaultInteraction: turnTableInteractionId
    }

    LinePlotLabelView {
        id: labelView
        anchors.fill: parent
        camera: renderer.camera
        region: rootData.region
    }

    LeadView {
        id: leadViewId
        anchors.fill: parent
        regionModel: rootData.regionTreeModel
        camera: renderer.camera
        visible: rootData.leadsVisible
    }

    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.right: parent.right
        anchors.rightMargin: 20
        spacing: 10

        ScaleBar {
            id: scaleBar
            visible: renderer.orthoProjection.enabled
            camera: renderer.camera
            anchors.bottom: compassItemId.bottom
            maxTotalWidth: renderer.width * 0.50
            minTotalWidth: renderer.height * 0.2
        }

        CompassItem {
            id: compassItemId
            width: 175
            height: width
            camera: renderer.camera
            rotation: turnTableInteractionId.rotation
            shaderDebugger: renderer.scene.shaderDebugger
            antialiasing: false
        }
    }
}

