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
    property alias turnTableInteraction: turnTableInteractionId
    property alias coordinatePickerInteraction: coordinatePickerId
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
            sampleCount: 4
        }
    }

    NeutralIconButton {
        id: pickButtonId
        objectName: "coordinatePickerButton"
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: 20
        }
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

    CoordinatePickerPopup {
        id: pickPopupId
        objectName: "coordinatePickerPopup"
        parent: rendererId
        picker: coordinatePickerId
        visible: coordinatePickerId.hasPick && pickButtonId.selected
    }

    // Toggling on while in perspective shows the ortho-prompt instead of
    // activating immediately — the clipper is top-down XY only.
    NeutralIconButton {
        id: lazClipButtonId
        objectName: "lazClipButton"
        anchors {
            left: pickButtonId.right
            bottom: parent.bottom
            leftMargin: 10
            bottomMargin: 20
        }
        iconSource: "qrc:/twbs-icons/icons/scissors.svg"
        sourceSize: Qt.size(21, 21)
        text: qsTr("Clip")
        toolTip: qsTr("Clip point cloud")
        selected: interactionManagerId.activeInteraction === lazClipInteractionId
        onClicked: {
            if (lazClipButtonId.selected) {
                lazClipInteractionId.deactivate()
            } else if (rendererId.orthoProjection.enabled) {
                lazClipInteractionId.activate()
            } else {
                orthoPromptId.visible = true
            }
        }
    }

    ShadowRectangle {
        id: orthoPromptId
        objectName: "lazClipOrthoPrompt"
        visible: false
        color: Theme.info
        radius: 5
        width: orthoPromptColumnId.width + 30
        height: orthoPromptColumnId.height + 20
        anchors.top: parent.top
        anchors.topMargin: 80
        anchors.horizontalCenter: parent.horizontalCenter

        Column {
            id: orthoPromptColumnId
            anchors.centerIn: parent
            spacing: 10

            QC.Label {
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: Theme.fontSizeUI
                text: qsTr("Clipping needs an orthographic top-down view.\nSwitch to ortho now?")
                horizontalAlignment: QC.Label.AlignHCenter
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10

                QC.Button {
                    objectName: "lazClipOrthoSwitchButton"
                    text: qsTr("Switch to ortho")
                    onClicked: {
                        rendererId.orthoProjection.enabled = true
                        orthoPromptId.visible = false
                        lazClipInteractionId.activate()
                    }
                }
                QC.Button {
                    objectName: "lazClipOrthoCancelButton"
                    text: qsTr("Cancel")
                    onClicked: {
                        orthoPromptId.visible = false
                    }
                }
            }
        }
    }
}

