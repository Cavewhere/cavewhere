import QtQuick
import QtTest
import QmlTestRecorder
import cavewherelib

Item {
    id: rootId
    objectName: "rootId"

    width: 400
    height: 600

    Sketch {
        id: sketchId
    }

    SketchCanvas {
        id: canvasId
        objectName: "canvas"
        anchors.fill: parent
        sketch: sketchId
    }

    SketchToolbar {
        id: toolbarId
        objectName: "sketchToolbar"
        sketch: sketchId
        canvas: canvasId
    }

    TestCase {
        name: "BrushPicker"
        when: windowShown

        function test_pickerBoundToResolvedPaletteAndCanvas() {
            const picker = findChild(toolbarId, "brushPicker")
            verify(picker !== null, "brush picker should exist")
            verify(picker.palette === sketchId.resolvedPalette,
                   "picker palette tracks the sketch's resolved palette")

            // brushName mirrors the canvas source of truth (3b).
            canvasId.currentBrushName = "feature"
            tryCompare(picker, "brushName", "feature", 2000)
        }

        function test_selectingBrushWritesCanvasAndExitsErase() {
            const picker = findChild(toolbarId, "brushPicker")
            toolbarId.eraseActive = true
            canvasId.currentBrushName = "wall"

            // Emitting the picker's selection signal exercises the toolbar's
            // wiring without driving the popup ListView geometry.
            picker.brushSelected("feature")

            compare(canvasId.currentBrushName, "feature",
                    "selecting a brush sets the canvas brush")
            verify(!toolbarId.eraseActive,
                   "selecting a brush exits eraser mode")
        }
    }
}
