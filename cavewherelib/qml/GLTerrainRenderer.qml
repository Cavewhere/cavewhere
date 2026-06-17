/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import cavewherelib


Item {
    id: rootId

    property alias turnTableInteraction: turnTableInteractionId
    property alias coordinatePickerInteraction: coordinatePickerId
    property alias interactionManager: interactionManagerId
    property alias leadView: leadViewId
    property alias renderer: rendererId
    property alias projectionTransition: projectionTransitionId
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
        // Don't set sampleCount; cwRenderingSettings drives it.
    }

    // PROTOTYPE SPIKE (#535/#536): depth-occluded QML billboard inside the RHI pass.
    BillboardTest {
        scene: rendererId.scene
        worldPosition: Qt.vector3d(0, 0, 0)
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
        pointCloudWorldRadiusTarget: RootData.regionSceneManager.lazLayersSceneNode
    }

    ProjectionTransition {
        id: projectionTransitionId
        camera: rendererId.camera
        orthoProjection: rendererId.orthoProjection
        perspectiveProjection: rendererId.perspectiveProjection
        interaction: turnTableInteractionId
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
    }

    LazClipInteractionView {
        id: lazClipInteractionId
        objectName: "lazClipInteraction"
        camera: rendererId.camera
        region: RootData.region
        lazLayersSceneNode: RootData.regionSceneManager.lazLayersSceneNode
        turnTableInteraction: turnTableInteractionId
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [
            turnTableInteractionId,
            coordinatePickerId,
            lazClipInteractionId
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
            // Don't set sampleCount; cwRenderingSettings drives it.
        }
    }

    // Floating background for the Pick/Clip toolbar. IconButton renders a
    // transparent background until hovered/selected, so without this surface
    // the icons disappear when the terrain underneath matches the icon color.
    ShadowRectangle {
        id: bottomToolbarId
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: 20
        }
        width: bottomToolbarRowId.implicitWidth + Theme.floatingToolbarPadding
        height: bottomToolbarRowId.implicitHeight + Theme.floatingToolbarPadding
        color: Theme.surface
        radius: 5

        RowLayout {
            id: bottomToolbarRowId
            anchors.centerIn: parent
            spacing: 4

            IconButton {
                id: pickButtonId
                objectName: "coordinatePickerButton"
                iconSource: "qrc:/twbs-icons/icons/crosshair.svg"
                sourceSize: Qt.size(21, 21)
                text: qsTr("Pick")
                toolTip: qsTr("Pick coordinates")
                selected: interactionManagerId.activeInteraction === coordinatePickerId
                onClicked: {
                    if (pickButtonId.selected) {
                        coordinatePickerId.deactivate()
                    } else {
                        coordinatePickerId.activate()
                    }
                }
            }

            IconButton {
                id: lazClipButtonId
                objectName: "lazClipButton"
                iconSource: "qrc:/twbs-icons/icons/scissors.svg"
                sourceSize: Qt.size(21, 21)
                text: qsTr("Clip")
                toolTip: qsTr("Clip point cloud")
                selected: interactionManagerId.activeInteraction === lazClipInteractionId
                onClicked: {
                    if (lazClipButtonId.selected) {
                        lazClipInteractionId.deactivate()
                    } else {
                        lazClipInteractionId.activate()
                    }
                }
            }
        }
    }

    CoordinatePickerPopup {
        id: pickPopupId
        objectName: "coordinatePickerPopup"
        parent: rendererId
        picker: coordinatePickerId
        visible: coordinatePickerId.hasPick && pickButtonId.selected
    }
}

