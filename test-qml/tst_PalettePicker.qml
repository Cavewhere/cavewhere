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
        name: "PalettePicker"
        when: windowShown

        function test_listsPalettesAndTracksResolved() {
            const combo = findChild(toolbarId, "paletteCombo")
            verify(combo !== null, "palette combo should exist")
            verify(combo.count >= 1, "the shipped default palette is listed")

            // currentIndex reflects the resolved palette (default fallback here),
            // not just the region's own id.
            tryVerify(() => combo.currentIndex >= 0, 2000,
                      "combo selects the resolved palette")
            compare(String(combo.currentValue),
                    String(sketchId.resolvedPalette.id),
                    "combo value tracks the resolved palette id")
        }

        function test_selectingWritesRegionId() {
            const combo = findChild(toolbarId, "paletteCombo")
            const previous = RootData.region.defaultPaletteId

            // activated() is the user-selection signal the combo emits; firing it
            // runs the toolbar's write-through to the region.
            combo.activated(combo.currentIndex)

            compare(String(RootData.region.defaultPaletteId),
                    String(combo.currentValue),
                    "selecting a palette writes the region's defaultPaletteId")

            RootData.region.defaultPaletteId = previous
        }
    }
}
