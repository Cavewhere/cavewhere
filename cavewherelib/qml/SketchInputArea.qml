/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Window
import cavewherelib

// The interactive view + input layer shared by the survey-sketch editor
// (SketchItem) and the glyph editor (GlyphEditorPage): the world<->screen
// matrix, the grid model, and the wheel/pinch/drag pan-zoom plus the pen and
// eraser handlers. It owns no canvas — the host places a SketchCanvas (or
// SketchGlyphCanvas) underneath and binds its view to the outputs here, so the
// same drawing feel works for either canvas type.
QQ.Item {
    id: inputAreaId

    // The sketch being drawn into. Drives view state (zoom/pan), stroke capture
    // and the world<->screen matrix.
    property Sketch sketch

    // Brush every new stroke is drawn with (the host's canvas.currentBrushName).
    property string brushName

    // Eraser is a tool mode, not a brush (Decision 2): when true, pen input
    // erases instead of drawing.
    property bool eraseActive: false

    // World-space eraser radius in meters; the on-screen disk is derived via
    // pxPerMeter so it matches the actual erase region at any zoom.
    property real eraserRadius: 0.5

    // Fresh sketches (viewInitialized === false) start centered at 1x; otherwise
    // each sketch remembers its own view.
    readonly property double zoom: sketch ? sketch.viewState.zoom : 1.0
    readonly property point _centeredPan: Qt.point(width / 2, height / 2)
    readonly property point pan: (sketch && sketch.viewState.viewInitialized)
        ? sketch.viewState.pan
        : _centeredPan

    // World-meters to screen-pixels at the current zoom. Wheel/pinch handlers and
    // the eraser-cursor size derive from this.
    readonly property double pxPerMeter: worldToScreenId.matrix.m11 * zoom

    readonly property bool zoomAllowed: sketch !== null && !sketch.viewState.zoomLocked

    // View bindings the host's canvas consumes.
    readonly property alias grid: gridModel
    readonly property alias worldToScreen: worldToScreenId
    readonly property int activeStrokeIndex: _activeStrokeIndex

    // Continuation tuning. The probation window is the minimum pen travel before
    // we decide commit vs reject — it gives the user room to retrace the
    // candidate and bounds how long an accidental pen-down can hijack a fresh
    // draw. The same window doubles as the commit blend zone.
    readonly property real _probationWindowScreenPx: 10 * Screen.pixelDensity

    property int _activeStrokeIndex: -1

    // Eraser mode state. _eraserCursor tracks the pen even while hovering so the
    // disk follows before pen-down; _eraserLastWorld seeds the per-move segment.
    property point _eraserCursor: Qt.point(0, 0)
    property point _eraserLastWorld: Qt.point(0, 0)
    property bool _eraserHasLastPoint: false

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

    // Publish the view matrix to the C++ view state so cwSketch can derive
    // screen-px->world-m thresholds without reimplementing the paper-scale math.
    onSketchChanged: {
        if (sketch) {
            sketch.viewState.worldToScreenMatrix = worldToScreenId
        }
    }

    WorldToScreenMatrix {
        id: worldToScreenId
        pixelDensity: Screen.pixelDensity
        // Mirror sketch.mapScale — the sketch owns the paper scale (1:250
        // default) and this matrix is a view-only consumer.
        scale.scaleNumerator.value:   inputAreaId.sketch ? inputAreaId.sketch.mapScale.scaleNumerator.value   : 1
        scale.scaleDenominator.value: inputAreaId.sketch ? inputAreaId.sketch.mapScale.scaleDenominator.value : 250
        scale.scaleNumerator.unit:    inputAreaId.sketch ? inputAreaId.sketch.mapScale.scaleNumerator.unit    : Units.Meters
        scale.scaleDenominator.unit:  inputAreaId.sketch ? inputAreaId.sketch.mapScale.scaleDenominator.unit  : Units.Meters
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

    // Probation commits inside cwSketch::appendPoint: the probation row is
    // removed and the candidate becomes the active row. We retarget
    // _activeStrokeIndex so the next appendPoint from onPointChanged lands on
    // the continued stroke.
    QQ.Connections {
        target: inputAreaId.sketch
        function onContinuationCommitted(newActiveStrokeIndex) {
            inputAreaId._activeStrokeIndex = newActiveStrokeIndex
        }
    }

    QQ.WheelHandler {
        enabled: inputAreaId.zoomAllowed
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        onWheel: (event) => {
            if (!inputAreaId.sketch || event.angleDelta.y === 0) {
                return
            }
            const factor = event.angleDelta.y > 0 ? 1.1 : 1.0 / 1.1
            const focus = Qt.point(event.x, event.y)
            const world = inputAreaId._worldPoint(focus)
            const newZoom = inputAreaId.zoom * factor
            // Re-solve pan so `world` stays under `focus` at the new zoom.
            const k = worldToScreenId.matrix.m11 * newZoom
            inputAreaId.sketch.viewState.zoom = newZoom
            inputAreaId.sketch.viewState.pan = Qt.point(focus.x - world.x * k,
                                                        focus.y + world.y * k)
        }
    }

    QQ.PinchHandler {
        id: pinchHandler
        enabled: inputAreaId.zoomAllowed
        target: null
        acceptedDevices: QQ.PointerDevice.TouchScreen | QQ.PointerDevice.TouchPad
        rotationAxis.enabled: false
        minimumPointCount: 2
        maximumPointCount: 2

        property double startZoom: 1.0
        property point startWorld: Qt.point(0, 0)

        function _applyGesture() {
            if (!inputAreaId.sketch) {
                return
            }
            const newZoom = startZoom * activeScale
            const k = worldToScreenId.matrix.m11 * newZoom
            const c = centroid.position
            inputAreaId.sketch.viewState.zoom = newZoom
            inputAreaId.sketch.viewState.pan = Qt.point(c.x - startWorld.x * k,
                                                        c.y + startWorld.y * k)
        }

        onActiveChanged: {
            if (active) {
                startZoom = inputAreaId.zoom
                startWorld = inputAreaId._worldPoint(centroid.position)
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
            if (!active || !inputAreaId.sketch) {
                return
            }
            inputAreaId.sketch.viewState.pan = Qt.point(
                inputAreaId.pan.x + centroid.position.x - lastPosition.x,
                inputAreaId.pan.y + centroid.position.y - lastPosition.y)
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
            if (!active || !inputAreaId.sketch) {
                return
            }
            inputAreaId.sketch.viewState.pan = Qt.point(
                inputAreaId.pan.x + centroid.position.x - lastPosition.x,
                inputAreaId.pan.y + centroid.position.y - lastPosition.y)
            lastPosition = centroid.position
        }
    }

    ExclusivePointHandler {
        id: penHandler
        objectName: "penHandler"
        target: null
        parent: inputAreaId
        acceptedDevices: QQ.PointerDevice.Stylus | QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        cursorShape: Qt.CrossCursor

        onActiveChanged: {
            if (!inputAreaId.sketch) {
                return
            }
            if (inputAreaId.eraseActive) {
                if (active) {
                    inputAreaId._eraserHasLastPoint = false
                } else {
                    // Close the merge window on pen-up so the next drag starts a
                    // fresh undo step.
                    inputAreaId.sketch.endEraseSession()
                    inputAreaId._eraserHasLastPoint = false
                }
                return
            }
            if (active) {
                // Endpoint-proximity landings skip probation entirely — within
                // one probation window of the tip there isn't enough line to
                // retrace for the hit-rate check to work.
                const worldStart = inputAreaId._worldPoint(point.position)
                const target = inputAreaId.sketch.findContinuationTarget(
                    inputAreaId.brushName, worldStart,
                    inputAreaId._probationWindowScreenPx)
                inputAreaId._activeStrokeIndex = inputAreaId.sketch.beginStroke(
                    inputAreaId.brushName)
                if (target.strokeIndex >= 0) {
                    if (target.endpoint !== 0) { // Endpoint.None
                        inputAreaId.sketch.commitAtEndpoint(
                            inputAreaId._activeStrokeIndex, target)
                    } else {
                        inputAreaId.sketch.armProbation(
                            inputAreaId._activeStrokeIndex,
                            target,
                            inputAreaId._probationWindowScreenPx)
                    }
                }
            } else {
                if (inputAreaId._activeStrokeIndex >= 0) {
                    inputAreaId.sketch.endStroke()
                    inputAreaId._activeStrokeIndex = -1
                }
            }
        }

        onPointChanged: {
            if (!inputAreaId.sketch) {
                return
            }
            if (inputAreaId.eraseActive) {
                inputAreaId._eraserCursor = point.position
                if (!active) {
                    return
                }
                const world = inputAreaId._worldPoint(point.position)
                if (inputAreaId._eraserHasLastPoint) {
                    // Feed just the new segment; the merging undo command
                    // stitches the whole drag into a single step.
                    inputAreaId.sketch.eraseAlongPath(
                        [inputAreaId._eraserLastWorld, world],
                        inputAreaId.eraserRadius)
                }
                inputAreaId._eraserLastWorld = world
                inputAreaId._eraserHasLastPoint = true
                return
            }
            if (!active) {
                return
            }
            if (inputAreaId._activeStrokeIndex < 0) {
                return
            }
            const pressure = point.pressure > 0.0 ? point.pressure : 1.0
            const world = inputAreaId._worldPoint(point.position)
            inputAreaId.sketch.appendPoint(inputAreaId._activeStrokeIndex,
                                           world, pressure, 0)
        }
    }

    // Eraser cursor: a hollow disk that tracks the pen while in eraser mode,
    // sized so the on-screen radius matches the actual erase region at any zoom.
    QQ.Rectangle {
        readonly property double radiusPx:
            inputAreaId.eraserRadius * inputAreaId.pxPerMeter

        visible: inputAreaId.eraseActive
                 && inputAreaId._eraserCursor.x !== 0
        width: radiusPx * 2
        height: radiusPx * 2
        radius: radiusPx
        color: "transparent"
        border.color: Theme.sketchGridLine
        border.width: 1
        x: inputAreaId._eraserCursor.x - radiusPx
        y: inputAreaId._eraserCursor.y - radiusPx
    }
}
