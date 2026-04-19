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

QQ.Item {
    id: sketchItemId

    objectName: "sketchItemId"

    property Sketch sketch
    property bool isNarrow: false

    property double zoom: 1.0
    property point pan: Qt.point(0, 0)

    readonly property int strokeKind: toolbarId.strokeKind
    readonly property double strokeWidth: strokeKind === PenStroke.Wall ? 4.0 : 2.5

    property int _activeStrokeIndex: -1

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
        scale.scaleNumerator.value: 1
        scale.scaleDenominator.value: 250
    }

    InfiniteGridModel {
        id: gridModel
        lineColor: Theme.sketchGridLine
        labelColor: Theme.sketchGridLabel
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
        zoom: sketchItemId.zoom
        pan: sketchItemId.pan
        mapMatrix: worldToScreenId.matrix
        activeStrokeIndex: sketchItemId._activeStrokeIndex
        grid: gridModel
    }

    QQ.WheelHandler {
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        onWheel: (event) => {
            if (event.angleDelta.y === 0) {
                return
            }
            const factor = event.angleDelta.y > 0 ? 1.1 : 1.0 / 1.1
            const focus = Qt.point(event.x, event.y)
            const world = sketchItemId._worldPoint(focus)
            sketchItemId.zoom = sketchItemId.zoom * factor
            // Re-solve pan so `world` stays under `focus` at the new zoom.
            const k = worldToScreenId.matrix.m11 * sketchItemId.zoom
            sketchItemId.pan = Qt.point(focus.x - world.x * k,
                                        focus.y + world.y * k)
        }
    }

    QQ.PinchHandler {
        id: pinchHandler
        target: null
        acceptedDevices: QQ.PointerDevice.TouchScreen | QQ.PointerDevice.TouchPad
        rotationAxis.enabled: false
        minimumPointCount: 2
        maximumPointCount: 2

        property double startZoom: 1.0
        property point startWorld: Qt.point(0, 0)

        function _applyGesture() {
            const newZoom = startZoom * activeScale
            const k = worldToScreenId.matrix.m11 * newZoom
            const c = centroid.position
            sketchItemId.zoom = newZoom
            sketchItemId.pan = Qt.point(c.x - startWorld.x * k,
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
            if (!active) {
                return
            }
            sketchItemId.pan = Qt.point(
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
            if (!active) {
                return
            }
            sketchItemId.pan = Qt.point(
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
            if (active) {
                sketchItemId._activeStrokeIndex = sketchItemId.sketch.beginStroke(
                    sketchItemId.strokeKind,
                    sketchItemId.strokeWidth)
            } else {
                if (sketchItemId._activeStrokeIndex >= 0) {
                    sketchItemId.sketch.endStroke()
                    sketchItemId._activeStrokeIndex = -1
                }
            }
        }

        onPointChanged: {
            if (!active || !sketchItemId.sketch) {
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

    SketchToolbar {
        id: toolbarId
        objectName: "sketchToolbar"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.pageMargin
        sketch: sketchItemId.sketch
    }
}
