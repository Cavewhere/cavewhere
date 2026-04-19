/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
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

    function _worldPoint(itemPoint) {
        return Qt.point((itemPoint.x - pan.x) / zoom,
                        (itemPoint.y - pan.y) / zoom)
    }

    clip: true

    InfiniteGridModel {
        id: gridModel
        lineColor: Theme.sketchGridLine
        labelColor: Theme.sketchGridLabel
        majorGridInterval: 100
        minorGridInterval: 20
        // The zoom-level heuristic's +2 offset assumes a cave-meter mapMatrix
        // (~15 px/m from the WorldToScreenMatrix prototype). With identity
        // mapMatrix we shift it so clampedZoomLevel == 0 at zoom == 1 and the
        // base intervals render 1:1 (100 / 20 px) instead of ×100.
        minorGridMinPixelInterval: 0.1
    }

    SketchCanvas {
        id: sketchCanvasId
        objectName: "sketchCanvas"
        anchors.fill: parent
        sketch: sketchItemId.sketch
        zoom: sketchItemId.zoom
        pan: sketchItemId.pan
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
            sketchItemId.pan = Qt.point(focus.x - world.x * sketchItemId.zoom,
                                        focus.y - world.y * sketchItemId.zoom)
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
