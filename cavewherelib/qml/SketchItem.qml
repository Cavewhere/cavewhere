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

    // Fresh sketches (viewInitialized === false) start centered at 1×; otherwise each sketch remembers its own view.
    readonly property double zoom: sketch ? sketch.viewState.zoom : 1.0
    readonly property point _centeredPan: Qt.point(width / 2, height / 2)
    readonly property point pan: (sketch && sketch.viewState.viewInitialized)
        ? sketch.viewState.pan
        : _centeredPan

    readonly property string brushName: toolbarId.brushName
    readonly property bool eraseActive: toolbarId.eraseActive
    readonly property bool zoomAllowed: sketch !== null && !sketch.viewState.zoomLocked

    // World-meters to screen-pixels for the current zoom. Wheel/pinch handlers
    // and the eraser-cursor size all derive from this.
    readonly property double _pxPerMeter: worldToScreenId.matrix.m11 * zoom

    // Continuation tuning. The probation window is the minimum pen travel
    // required before we decide commit vs reject — it both gives the user
    // room to retrace the candidate and bounds how long an accidental
    // pen-down on a stroke can hijack a fresh draw. The same window also
    // serves as the blend zone on commit: probation samples are replayed
    // as lerp(oldCenterline, rawPen, t) with t swept 0→1 across it, so the
    // user's overdraw smooths into the existing line.
    readonly property real _probationWindowScreenPx: 10 * Screen.pixelDensity

    property int _activeStrokeIndex: -1

    // Eraser mode state. _eraserCursor tracks the pen even while hovering so
    // the cursor disk follows before pen-down. _eraserLastWorld seeds the
    // per-move segment passed to eraseAlongPath.
    property point _eraserCursor: Qt.point(0, 0)
    property point _eraserLastWorld: Qt.point(0, 0)
    property bool _eraserHasLastPoint: false

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
    // active drawing starts). Also publishes the worldToScreenMatrix to the
    // C++ view state so cwSketch can derive screen-px→world-m thresholds
    // without reimplementing the paper-scale formula.
    onSketchChanged: {
        if (_previousSketch !== null && _previousSketch !== sketch) {
            RootData.sketchManager.flushIconIfDirty(_previousSketch)
        }
        if (sketch !== null && sketch !== _previousSketch) {
            RootData.sketchManager.flushIconIfDirty(sketch)
        }
        _previousSketch = sketch

        if (sketch) {
            sketch.viewState.worldToScreenMatrix = worldToScreenId
        }
    }

    function _handleAnchorSelectionRequest(componentList) {
        _pendingAnchorComponents = componentList
        if (_pageReady) {
            anchorPickerLoaderId.active = true
        }
    }

    // mapMatrix is diag(mapScale, -mapScale, mapScale) — the Y-flip makes world
    // Y-up while the item stays Y-down — so the forward/inverse transforms are
    // simple scalar math rather than full matrix inversions.
    function _worldPoint(itemPoint) {
        const mapScale = worldToScreenId.matrix.m11
        if (mapScale === 0.0) {
            return Qt.point(0, 0)
        }
        return Qt.point((itemPoint.x - pan.x) / zoom / mapScale,
                        -(itemPoint.y - pan.y) / zoom / mapScale)
    }

    clip: true

    WorldToScreenMatrix {
        id: worldToScreenId
        pixelDensity: Screen.pixelDensity
        // Mirror sketch.mapScale — the sketch owns the paper scale (1:250
        // default) and this matrix is a view-only consumer.
        scale.scaleNumerator.value:   sketchItemId.sketch ? sketchItemId.sketch.mapScale.scaleNumerator.value   : 1
        scale.scaleDenominator.value: sketchItemId.sketch ? sketchItemId.sketch.mapScale.scaleDenominator.value : 250
        scale.scaleNumerator.unit:    sketchItemId.sketch ? sketchItemId.sketch.mapScale.scaleNumerator.unit    : Units.Meters
        scale.scaleDenominator.unit:  sketchItemId.sketch ? sketchItemId.sketch.mapScale.scaleDenominator.unit  : Units.Meters
    }

    // Probation commits inside cwSketch::appendPoint: the probation row is
    // removed and the candidate becomes the active row. We have to retarget
    // _activeStrokeIndex so the next appendPoint from onPointChanged lands
    // on the continued stroke.
    QQ.Connections {
        target: sketchItemId.sketch
        function onContinuationCommitted(newActiveStrokeIndex) {
            sketchItemId._activeStrokeIndex = newActiveStrokeIndex
        }
    }

    InfiniteGridModel {
        id: gridModel
        lineColor: Theme.sketchGridLine
        labelColor: Theme.sketchGridLabel
        labelBackgroundColor: Theme.sketchGridLabelBackground
        labelFont: Qt.font({ family: Theme.fontFamily, pixelSize: Theme.fontSizeSmall })
        majorGridInterval: 5
        minorGridInterval: 1
        mapMatrix: worldToScreenId.matrix
    }

    SketchCanvas {
        id: sketchCanvasId
        objectName: "sketchCanvas"
        anchors.fill: parent
        sketch: sketchItemId.sketch
        sketchManager: RootData.sketchManager
        scrapManager: RootData.scrapManager
        debugOverlayVisible: debugToggleId.checked
        zoom: sketchItemId.zoom
        pan: sketchItemId.pan
        mapMatrix: worldToScreenId.matrix
        activeStrokeIndex: sketchItemId._activeStrokeIndex
        grid: gridModel

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

    QQ.WheelHandler {
        enabled: sketchItemId.zoomAllowed
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        onWheel: (event) => {
            if (!sketchItemId.sketch || event.angleDelta.y === 0) {
                return
            }
            const factor = event.angleDelta.y > 0 ? 1.1 : 1.0 / 1.1
            const focus = Qt.point(event.x, event.y)
            const world = sketchItemId._worldPoint(focus)
            const newZoom = sketchItemId.zoom * factor
            // Re-solve pan so `world` stays under `focus` at the new zoom.
            const k = worldToScreenId.matrix.m11 * newZoom
            sketchItemId.sketch.viewState.zoom = newZoom
            sketchItemId.sketch.viewState.pan = Qt.point(focus.x - world.x * k,
                                                         focus.y + world.y * k)
        }
    }

    QQ.PinchHandler {
        id: pinchHandler
        enabled: sketchItemId.zoomAllowed
        target: null
        acceptedDevices: QQ.PointerDevice.TouchScreen | QQ.PointerDevice.TouchPad
        rotationAxis.enabled: false
        minimumPointCount: 2
        maximumPointCount: 2

        property double startZoom: 1.0
        property point startWorld: Qt.point(0, 0)

        function _applyGesture() {
            if (!sketchItemId.sketch) {
                return
            }
            const newZoom = startZoom * activeScale
            const k = worldToScreenId.matrix.m11 * newZoom
            const c = centroid.position
            sketchItemId.sketch.viewState.zoom = newZoom
            sketchItemId.sketch.viewState.pan = Qt.point(c.x - startWorld.x * k,
                                                         c.y + startWorld.y * k)
        }

        onActiveChanged: {
            if (active) {
                startZoom = sketchItemId.zoom
                startWorld = sketchItemId._worldPoint(centroid.position)
            }
        }

        onActiveScaleChanged: if (active) _applyGesture()
        onCentroidChanged: if (active) _applyGesture()
    }

    QQ.DragHandler {
        id: touchPanHandler
        target: null
        acceptedDevices: QQ.PointerDevice.TouchScreen
        maximumPointCount: 1
        dragThreshold: 0

        property point lastPosition

        onActiveChanged: {
            if (active) {
                lastPosition = centroid.position
            }
        }

        onCentroidChanged: {
            if (!active || !sketchItemId.sketch) {
                return
            }
            sketchItemId.sketch.viewState.pan = Qt.point(
                sketchItemId.pan.x + centroid.position.x - lastPosition.x,
                sketchItemId.pan.y + centroid.position.y - lastPosition.y)
            lastPosition = centroid.position
        }
    }

    QQ.DragHandler {
        id: panHandler
        target: null
        acceptedButtons: Qt.RightButton | Qt.MiddleButton
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        dragThreshold: 0

        property point lastPosition

        onActiveChanged: {
            if (active) {
                lastPosition = centroid.position
            }
        }

        onCentroidChanged: {
            if (!active || !sketchItemId.sketch) {
                return
            }
            sketchItemId.sketch.viewState.pan = Qt.point(
                sketchItemId.pan.x + centroid.position.x - lastPosition.x,
                sketchItemId.pan.y + centroid.position.y - lastPosition.y)
            lastPosition = centroid.position
        }
    }

    ExclusivePointHandler {
        id: penHandler
        objectName: "penHandler"
        target: null
        parent: sketchItemId
        acceptedDevices: QQ.PointerDevice.Stylus | QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        cursorShape: Qt.CrossCursor

        onActiveChanged: {
            if (!sketchItemId.sketch) {
                return
            }
            if (sketchItemId.eraseActive) {
                if (active) {
                    sketchItemId._eraserHasLastPoint = false
                } else {
                    // Close the merge window on pen-up so the next drag
                    // starts a fresh undo step.
                    sketchItemId.sketch.endEraseSession()
                    sketchItemId._eraserHasLastPoint = false
                }
                return
            }
            if (active) {
                // Endpoint-proximity landings skip probation entirely —
                // within one probation window of the tip there isn't
                // enough line to retrace for the hit-rate check to work.
                const worldStart = sketchItemId._worldPoint(point.position)
                const target = sketchItemId.sketch.findContinuationTarget(
                    sketchItemId.brushName, worldStart,
                    sketchItemId._probationWindowScreenPx)
                sketchItemId._activeStrokeIndex = sketchItemId.sketch.beginStroke(
                    sketchItemId.brushName)
                if (target.strokeIndex >= 0) {
                    if (target.endpoint !== 0) { // Endpoint.None
                        sketchItemId.sketch.commitAtEndpoint(
                            sketchItemId._activeStrokeIndex, target)
                    } else {
                        sketchItemId.sketch.armProbation(
                            sketchItemId._activeStrokeIndex,
                            target,
                            sketchItemId._probationWindowScreenPx)
                    }
                }
            } else {
                if (sketchItemId._activeStrokeIndex >= 0) {
                    sketchItemId.sketch.endStroke()
                    sketchItemId._activeStrokeIndex = -1
                }
            }
        }

        onPointChanged: {
            if (!sketchItemId.sketch) {
                return
            }
            if (sketchItemId.eraseActive) {
                sketchItemId._eraserCursor = point.position
                if (!active) {
                    return
                }
                const world = sketchItemId._worldPoint(point.position)
                if (sketchItemId._eraserHasLastPoint) {
                    // Feed just the new segment; the merging undo command
                    // stitches the whole drag into a single step.
                    sketchItemId.sketch.eraseAlongPath(
                        [sketchItemId._eraserLastWorld, world],
                        toolbarId.eraserRadius)
                }
                sketchItemId._eraserLastWorld = world
                sketchItemId._eraserHasLastPoint = true
                return
            }
            if (!active) {
                return
            }
            if (sketchItemId._activeStrokeIndex < 0) {
                return
            }
            const pressure = point.pressure > 0.0 ? point.pressure : 1.0
            const world = sketchItemId._worldPoint(point.position)
            sketchItemId.sketch.appendPoint(sketchItemId._activeStrokeIndex,
                                            world, pressure, 0)
        }
    }

    // Eraser cursor: a hollow disk that tracks the pen while in eraser mode,
    // sized so the on-screen radius matches the actual erase region at any
    // zoom. Visible while the pen hovers or drags — ExclusivePointHandler
    // updates point.position in both cases.
    QQ.Rectangle {
        readonly property double radiusPx:
            toolbarId.eraserRadius * sketchItemId._pxPerMeter

        visible: sketchItemId.eraseActive
                 && sketchItemId._eraserCursor.x !== 0
        width: radiusPx * 2
        height: radiusPx * 2
        radius: radiusPx
        color: "transparent"
        border.color: Theme.sketchGridLine
        border.width: 1
        x: sketchItemId._eraserCursor.x - radiusPx
        y: sketchItemId._eraserCursor.y - radiusPx
    }

    SketchToolbar {
        id: toolbarId
        objectName: "sketchToolbar"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Theme.pageMargin
        sketch: sketchItemId.sketch
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
