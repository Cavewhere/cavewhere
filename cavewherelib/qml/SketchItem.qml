/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Window
import cavewherelib

QQ.Item {
    id: sketchItemId

    objectName: "sketchItemId"

    property Sketch sketch
    property bool isNarrow: false

    property Sketch _previousSketch: null

    readonly property point _centeredPan: Qt.point(width / 2, height / 2)

    // Gates the anchor-picker dialog. We only open it after this item has
    // fully loaded, so transient sketch swaps during page construction do
    // not flash a modal. See plan §7.
    property bool _pageReady: false
    property var  _pendingAnchorComponents: null

    QQ.Component.onCompleted: {
        _pageReady = true
        if (_pendingAnchorComponents !== null) {
            anchorPickerLoaderId.active = true
        }
    }

    // Navigation-triggered flush. When the bound sketch changes, we flush
    // both the sketch being left (so its thumbnail catches up for the
    // gallery) and the sketch being entered (covers the case where a prior
    // session closed without flushing — no-op otherwise, and gated when
    // active drawing starts).
    onSketchChanged: {
        if (_previousSketch !== null && _previousSketch !== sketch) {
            RootData.sketchManager.flushIconIfDirty(_previousSketch)
        }
        if (sketch !== null && sketch !== _previousSketch) {
            RootData.sketchManager.flushIconIfDirty(sketch)
        }
        _previousSketch = sketch
    }

    function _handleAnchorSelectionRequest(componentList) {
        _pendingAnchorComponents = componentList
        if (_pageReady) {
            anchorPickerLoaderId.active = true
        }
    }

    clip: true

    SketchCanvas {
        id: sketchCanvasId
        objectName: "sketchCanvas"
        anchors.fill: parent
        sketch: sketchItemId.sketch
        sketchManager: RootData.sketchManager
        scrapManager: RootData.scrapManager
        debugOverlayVisible: debugToggleId.checked
        zoom: inputAreaId.zoom
        pan: inputAreaId.pan
        mapMatrix: inputAreaId.worldToScreen.matrix
        activeStrokeIndex: inputAreaId.activeStrokeIndex
        grid: inputAreaId.grid

        // Finished strokes take per-brush ink from the palette snapshot; this
        // one token only colors the neutral in-progress stroke preview.
        pathModel.activeStrokeColor: Theme.sketchStrokeWall
        linePlotModel.stationColor: Theme.sketchStation
        linePlotModel.shotLineColor: Theme.sketchShotLine

        onRequestAnchorSelection: (components) => {
            sketchItemId._handleAnchorSelectionRequest(components)
        }
    }

    QC.CheckBox {
        id: debugToggleId
        objectName: "debugScrapSelectionToggle"

        text: "Debug scrap selection"
        checked: false

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 4
    }

    QQ.Loader {
        id: anchorPickerLoaderId
        objectName: "anchorPickerLoader"
        active: false

        sourceComponent: ChooseTripComponentDialog {
            sketch: sketchItemId.sketch
            QQ.Component.onCompleted: {
                if (sketchItemId._pendingAnchorComponents !== null) {
                    open(sketchItemId._pendingAnchorComponents)
                    sketchItemId._pendingAnchorComponents = null
                }
            }
            onAnchorChosen: {
                anchorPickerLoaderId.active = false
            }
        }
    }

    SketchInputArea {
        id: inputAreaId
        objectName: "sketchInputArea"
        anchors.fill: parent
        sketch: sketchItemId.sketch
        brushName: sketchCanvasId.currentBrushName
        eraseActive: toolbarId.eraseActive
        eraserRadius: toolbarId.eraserRadius
    }

    SketchToolbar {
        id: toolbarId
        objectName: "sketchToolbar"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Theme.pageMargin
        sketch: sketchItemId.sketch
        canvas: sketchCanvasId
    }

    RowLayoutPanel {
        id: scalePanelId
        objectName: "sketchScalePanel"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: Theme.pageMargin
        visible: sketchItemId.sketch !== null

        ScaleInput {
            id: mapScaleInputId
            objectName: "mapScaleInput"
            onPaperLabel: "On Paper"
            inCaveLabel: "In Cave"
            valueVisible: true
            onPaperValue: sketchItemId.sketch ? sketchItemId.sketch.mapScale.scaleNumerator   : null
            inCaveValue:  sketchItemId.sketch ? sketchItemId.sketch.mapScale.scaleDenominator : null
            scaleValue:   sketchItemId.sketch ? sketchItemId.sketch.mapScale.scale            : 1.0
        }

        LockButton {
            id: zoomLockButtonId
            objectName: "zoomLockButton"
            checkable: true
            checked: sketchItemId.sketch !== null && sketchItemId.sketch.viewState.zoomLocked
            QC.ToolTip.text: checked ? "Unlock zoom" : "Lock zoom to map scale"
            QC.ToolTip.visible: hovered
            enabled: sketchItemId.sketch !== null
            onToggled: {
                sketchItemId.sketch.viewState.zoomLocked = checked
                if (checked) {
                    zoomResetAnimationId.restart()
                }
            }
        }
    }

    // Reset both zoom AND pan — animating only zoom left the world origin at the user's last pan offset (issue #457).
    QQ.ParallelAnimation {
        id: zoomResetAnimationId

        readonly property var viewState: sketchItemId.sketch ? sketchItemId.sketch.viewState : null

        QQ.NumberAnimation {
            target: zoomResetAnimationId.viewState
            property: "zoom"
            to: 1.0
            duration: 250
            easing.type: Easing.InOutQuad
        }

        QQ.PropertyAnimation {
            target: zoomResetAnimationId.viewState
            property: "pan"
            to: sketchItemId._centeredPan
            duration: 250
            easing.type: Easing.InOutQuad
        }
    }
}
