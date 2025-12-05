/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick
import cavewherelib
import QtQuick.Window


Item {
    property alias turnTableInteraction: turnTableInteractionId
    property alias interactionManager: interactionManagerId
    property alias leadView: leadViewId
    property alias renderer: rendererId
    property alias scene: rendererId.scene

    clip: true

    RegionViewer {
        id: rendererId
        anchors.fill: parent
        camera.devicePixelRatio: Screen.devicePixelRatio
        sampleCount:4
    }

    TurnTableInteraction {
        id: turnTableInteractionId
        objectName: "turnTableInteraction"
        anchors.fill: parent
        camera: rendererId.camera
        scene: rendererId.scene
        gridPlane: RootData.regionSceneManager.gridPlane.plane
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
        camera: rendererId.camera
        region: RootData.region
        visible: RootData.stationsVisible
    }

    LeadView {
        id: leadViewId
        anchors.fill: parent
        regionModel: RootData.regionTreeModel
        camera: rendererId.camera
        visible: RootData.leadsVisible
    }

    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.right: parent.right
        anchors.rightMargin: 20
        spacing: 10

        ScaleBar {
            id: scaleBar
            visible: rendererId.orthoProjection.enabled
            camera: rendererId.camera
            anchors.bottom: compassItemId.bottom
            maxTotalWidth: rendererId.width * 0.50
            minTotalWidth: rendererId.height * 0.2
        }

        Compass {
            id: compassItemId
            width: 175
            height: width
            compassRotation: turnTableInteractionId.cameraRotation
            sampleCount: 4
        }
    }
}

