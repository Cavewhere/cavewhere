/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick
import QtQuick.Controls as QC
import QtQuick.Window
import cavewherelib


Item {
    id: rootId

    property alias turnTableInteraction: turnTableInteractionId
    property alias coordinatePickerInteraction: coordinatePickerId
    property alias interactionManager: interactionManagerId
    property alias leadView: leadViewId
    property alias renderer: rendererId
    property alias scene: rendererId.scene

    clip: true

    // Keys handlers fire only when this item has active focus.
    focus: true

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_P && !event.isAutoRepeat) {
            turnTableInteractionId.pKeyHeld = true
            event.accepted = true
        }
    }

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_P && !event.isAutoRepeat) {
            turnTableInteractionId.pKeyHeld = false
            event.accepted = true
        }
    }

    // Pull focus on hover so hold-P + wheel works without an explicit click.
    HoverHandler {
        onHoveredChanged: {
            if (hovered) {
                rootId.forceActiveFocus()
            }
        }
    }

    RegionViewer {
        id: rendererId
        anchors.fill: parent
        camera.devicePixelRatio: Screen.devicePixelRatio
        sampleCount:4
    }

    Binding {
        target: RootData.regionSceneManager.background
        property: "color1"
        value: Theme.viewBackground.gradientInner
    }

    Binding {
        target: RootData.regionSceneManager.background
        property: "color2"
        value: Theme.viewBackground.gradientOuter
    }

    Binding {
        target: RootData.regionSceneManager.gridPlane
        property: "color"
        value: Theme.viewBackground.gridLineColor
    }

    TurnTableInteraction {
        id: turnTableInteractionId
        objectName: "turnTableInteraction"
        anchors.fill: parent
        camera: rendererId.camera
        scene: rendererId.scene
        gridPlane: RootData.regionSceneManager.gridPlane.plane
    }

    // While the picker is the active Interaction, the turn-table is disabled.
    // The picker forwards rotate + zoom gestures to the turn-table so the
    // user can still orient the camera; left-click is reserved for picking.
    CoordinatePickerInteraction {
        id: coordinatePickerId
        objectName: "coordinatePicker"
        camera: rendererId.camera
        scene: rendererId.scene
        region: RootData.region
        turnTableInteraction: turnTableInteractionId

        onDeactivated: pickButtonId.checked = false
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [
            turnTableInteractionId,
            coordinatePickerId
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
            objectName: "compass"
            width: Math.min(Math.max(rendererId.width * 0.25, 80), 175)
            height: width
            compassRotation: turnTableInteractionId.cameraRotation
            sampleCount: 4
        }
    }

    QC.RoundButton {
        id: pickButtonId
        objectName: "coordinatePickerButton"
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: 20
        }
        checkable: true
        text: "⌖"
        font.pixelSize: Theme.fontSizeTitle
        QC.ToolTip.visible: hovered
        QC.ToolTip.text: qsTr("Pick coordinates")
        // Guard: when deactivate() runs externally (e.g. another interaction
        // takes over), onDeactivated below sets checked = false; this guard
        // prevents the toggle from calling deactivate() again and re-emitting.
        onCheckedChanged: {
            if (checked && interactionManagerId.activeInteraction !== coordinatePickerId) {
                coordinatePickerId.activate()
            } else if (!checked && interactionManagerId.activeInteraction === coordinatePickerId) {
                coordinatePickerId.deactivate()
            }
        }
    }

    CoordinatePickerPopup {
        id: pickPopupId
        objectName: "coordinatePickerPopup"
        parent: rendererId
        picker: coordinatePickerId
        visible: coordinatePickerId.hasPick && pickButtonId.checked
    }
}

