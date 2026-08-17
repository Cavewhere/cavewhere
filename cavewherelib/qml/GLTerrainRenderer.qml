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
import "Utils.js" as Utils


Item {
    id: rootId

    property alias turnTableInteraction: turnTableInteractionId
    property alias coordinatePickerInteraction: coordinatePickerId
    property alias lazClipInteraction: lazClipInteractionId
    property alias measurementInteraction: measurementInteractionId
    property alias measurementReadoutPopup: measurementPopupId
    property alias interactionManager: interactionManagerId
    property alias leadView: leadViewId
    property alias renderer: rendererId
    property alias projectionTransition: projectionTransitionId
    property alias scene: rendererId.scene

    // The View page's tool model (per-page tool contract). The sidebar tool rail
    // renders this; grouping and order here drive the rail's grouping and order.
    property list<ToolItem> tools: [
        ToolItem {
            buttonObjectName: "coordinatePickerButton"
            interaction: coordinatePickerId
            iconSource: "qrc:/twbs-icons/icons/crosshair.svg"
            text: qsTr("Pick")
            toolTip: qsTr("Pick coordinates")
            group: qsTr("Pick & Measure")
            groupId: "measurePick"
        },
        ToolItem {
            buttonObjectName: "measurementButton"
            interaction: measurementInteractionId
            iconSource: "qrc:/twbs-icons/icons/rulers.svg"
            text: qsTr("Measure")
            toolTip: qsTr("Measure distance and bearing")
            group: qsTr("Pick & Measure")
            groupId: "measurePick"
        },
        ToolItem {
            buttonObjectName: "lazClipButton"
            interaction: lazClipInteractionId
            iconSource: "qrc:/twbs-icons/icons/scissors.svg"
            text: qsTr("Clip")
            flyoutTitle: qsTr("Point Cloud Clip")
            toolTip: qsTr("Clip point cloud")
            group: qsTr("Point Cloud")
            groupId: "pointCloud"
            // The Crop/Erase/Cancel actions live in the sidebar tool-property
            // flyout while Clip is armed, not floating over the scene.
            propertyContent: Component {
                LazClipToolOptions {
                    interaction: lazClipInteractionId
                }
            }
        }
    ]

    // Explicit overlay stacking so order is deterministic instead of
    // declaration-order dependent. Bottom to top: the 3D scene, interaction
    // graphics drawn over it, survey labels/leads, then the floating chrome
    // (compass, scale bar, tool buttons). The readout popups are QC.Popup and
    // render in the window overlay above all of these, so they need no z.
    readonly property int zScene: 0
    readonly property int zInteraction: 1
    readonly property int zLabels: 2
    // Top of the stack: in-view chrome (compass + scale bar, and the floating
    // Camera/Layers/Tools buttons the View page reparents in at narrow widths).
    // Chrome has to sit above zLabels, not merely above the scene: LeadView and
    // LinePlotLabelView fill this item and carry tap-away handlers, so anything
    // clickable left below them never sees a press.
    readonly property int zOverlay: 3

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
        z: rootId.zScene
        camera.devicePixelRatio: Screen.devicePixelRatio
        // Don't set sampleCount; cwRenderingSettings drives it.
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
        z: rootId.zInteraction
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
        z: rootId.zInteraction
        camera: rendererId.camera
        scene: rendererId.scene
        geoReference: RootData.region.geoReference
        // Numbers copied out of the readout are usually on their way into a fix,
        // which defaults to this same datum — so they arrive consistent.
        datum: RootData.region.defaultFixDatum
        turnTableInteraction: turnTableInteractionId
    }

    LazClipInteractionView {
        id: lazClipInteractionId
        objectName: "lazClipInteraction"
        z: rootId.zInteraction
        camera: rendererId.camera
        region: RootData.region
        lazLayersSceneNode: RootData.regionSceneManager.lazLayersSceneNode
        turnTableInteraction: turnTableInteractionId
    }

    MeasurementInteractionView {
        id: measurementInteractionId
        objectName: "measurementInteraction"
        z: rootId.zInteraction
        camera: rendererId.camera
        scene: rendererId.scene
        geoReference: RootData.region.geoReference
        turnTableInteraction: turnTableInteractionId
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [
            turnTableInteractionId,
            coordinatePickerId,
            lazClipInteractionId,
            measurementInteractionId
        ]
        defaultInteraction: turnTableInteractionId
    }

    // Armed from a fix-station surface rather than from the tool rail: it exists
    // to answer a question another page asked (FixStationPick).
    FixStationPickTool {
        objectName: "fixStationPickTool"
        view: rootId
    }

    LinePlotLabelView {
        id: labelView
        anchors.fill: parent
        z: rootId.zLabels
        camera: rendererId.camera
        scene: rendererId.scene
        region: RootData.region
        keywordItemModel: RootData.keywordItemModel
        visible: RootData.stationsVisible
    }

    LeadView {
        id: leadViewId
        anchors.fill: parent
        z: rootId.zLabels
        regionModel: RootData.regionTreeModel
        camera: rendererId.camera
        scene: rendererId.scene
        keywordItemModel: RootData.keywordItemModel
        visible: RootData.leadsVisible

        // Tap on empty space closes the open lead popup (dialog dismiss). Lead
        // items grab their taps exclusively, so this only fires off a lead.
        TapHandler {
            onTapped: leadViewId.selectionManager.selectedItem = null
        }
    }

    // cwLinkGenerator owns the page-address scheme, so the banner's link doesn't
    // hardcode the page tree.
    LinkGenerator {
        id: outlierLinkGeneratorId
        pageSelectionModel: RootData.pageSelectionModel
    }

    // A fix-station coordinate typo puts one station thousands of kilometers
    // from the rest and blows the scene bounds up until the cave is a sub-pixel
    // dot — the 3D view just looks empty. This top banner names the culprit so
    // an empty render has a cause the user can act on, instead of looking like a
    // bug, and links straight to the offending cave's fix stations to fix it.
    ErrorHelpBox {
        id: fixStationOutlierBox
        objectName: "fixStationOutlierBox"
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.bottom: undefined
        // The banner carries a link, so it needs the chrome layer, not just a
        // spot above the scene: LeadView's tap-away handler fills this item at
        // zLabels and takes the press first, leaving the link unclickable.
        z: rootId.zOverlay
        visible: RootData.region.fixStationValidator.warningMessage !== ""
        // Markup here is real — the link is what makes the banner actionable —
        // so the message it is glued to has to be escaped: the summary quotes
        // the offending cave's name, and the user chose that name.
        textFormat: QC.Label.RichText
        text: Utils.escapeHtml(RootData.region.fixStationValidator.warningMessage)
              + qsTr("<br>Open the cave's <a href=\"fixStations\">Fix Stations</a> to correct the coordinate.")

        onLinkActivated: {
            let cave = RootData.region.fixStationValidator.firstOutlierCave
            if (cave !== null) {
                RootData.pageSelectionModel.currentPageAddress = outlierLinkGeneratorId.fixStationsLink(cave)
            }
        }
    }

    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.right: parent.right
        anchors.rightMargin: 20
        z: rootId.zOverlay
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

    // The tools are armed from the main sidebar's tool rail (see MainSideBar),
    // which renders the `tools` model above. The picked/measured readout popups
    // stay in the view, keyed off the InteractionManager's active tool.
    CoordinatePickerPopup {
        id: pickPopupId
        objectName: "coordinatePickerPopup"
        parent: rendererId
        picker: coordinatePickerId
        // Silent during a fix-station pick: that click is answered by writing
        // the fix and returning to the page it came from, so a readout to copy
        // numbers out of would only flash on the way out.
        visible: coordinatePickerId.hasPick
                 && interactionManagerId.activeInteraction === coordinatePickerId
                 && !FixStationPick.active
    }

    MeasurementReadoutPopup {
        id: measurementPopupId
        objectName: "measurementReadoutPopup"
        parent: rendererId
        interaction: measurementInteractionId
        visible: interactionManagerId.activeInteraction === measurementInteractionId
                 && measurementInteractionId.hasMeasurement
        x: Math.max(0, parent.width - width - 20)
        y: 20
    }
}

