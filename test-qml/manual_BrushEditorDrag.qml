import QtQuick
import QtTest
import QmlTestRecorder
import cavewherelib

// Manual interaction harness for the brush-editor rule drag handler.
//
// This is NOT an automated test: it sets the scene up (draws wall / floor-step /
// ceiling-step strokes, opens the editor on ceiling-step, and seeds a second
// Placement rule so a category has >= 2 rules and the drag handle appears) and
// then sits in a long wait() so you can drag rule rows by hand and watch the
// drop-lines and model.
//
// The file is deliberately named "manual_*" rather than "tst_*" so Qt Quick
// Test's directory auto-discovery skips it (the long wait would hang the full
// suite). Run it explicitly, in a REAL window (no --platform offscreen):
//
//   QSG_RHI_BACKEND=metal ./build/<preset>/cavewhere-qml-test \
//       -input test-qml/manual_BrushEditorDrag.qml
//
// Ctrl-C to quit early.
Item {
    id: rootId
    objectName: "rootId"

    width: 800
    height: 900

    Sketch {
        id: sketchId
    }

    SketchItem {
        id: sketchItemId
        objectName: "sketchItem"
        anchors.fill: parent
        sketch: sketchId
    }

    TestCase {
        name: "BrushEditorDragPlayground"
        when: windowShown

        function drawStroke(brushName, points) {
            const canvas = findChild(sketchItemId, "sketchCanvas")
            canvas.currentBrushName = brushName
            mousePress(sketchItemId, points[0].x, points[0].y, Qt.LeftButton)
            for (let i = 1; i < points.length; i++) {
                mouseMove(sketchItemId, points[i].x, points[i].y, 0, Qt.LeftButton)
            }
            const last = points[points.length - 1]
            mouseRelease(sketchItemId, last.x, last.y, Qt.LeftButton)
        }

        function test_dragPlayground() {
            const canvas = findChild(sketchItemId, "sketchCanvas")
            verify(canvas !== null, "sketchCanvas should exist")
            tryVerify(() => sketchId.resolvedPalette !== null, 4000,
                      "palette should resolve before drawing")

            // Three horizontal strokes, stacked, so they're easy to tell apart.
            drawStroke("wall",         [Qt.point(180, 200), Qt.point(280, 200), Qt.point(380, 200)])
            drawStroke("floor-step",   [Qt.point(180, 350), Qt.point(280, 350), Qt.point(380, 350)])
            drawStroke("ceiling-step", [Qt.point(180, 500), Qt.point(280, 500), Qt.point(380, 500)])
            tryVerify(() => sketchId.strokeCount() === 3, 2000, "three strokes drawn")

            // Right-click the ceiling-step stroke to open the editor on it.
            mouseClick(sketchItemId, 280, 500, Qt.RightButton)

            const panel = findChild(sketchItemId, "brushEditorPanel")
            verify(panel !== null, "brush editor panel should exist")
            tryVerify(() => panel.visible, 2000, "panel opens on right-click")

            const editor = findChild(panel, "brushEditor")
            verify(editor !== null, "editor controller should exist")
            tryVerify(() => editor.loaded && editor.brushName === "ceiling-step", 2000,
                      "editor loads the ceiling-step brush")

            // ceiling-step's tick layer (decoration index 1) ships one rule per
            // category, so nothing is reorderable out of the box. It already has
            // "Rigid stamp" in the Output category; add two more distinctly-named
            // Output rules so that category has three rows whose order you can
            // actually see change when you drag them.
            const tickLayer = 1
            const before = editor.ruleCount(tickLayer)
            editor.addRule(tickLayer, "Bending stamp")
            editor.addRule(tickLayer, "Jointed stamp")
            tryVerify(() => editor.ruleCount(tickLayer) === before + 2, 2000,
                      "two more Output rules are seeded for dragging")

            console.log("=== Brush editor drag playground ready ===")
            console.log("Editor open on:", editor.brushName)
            console.log("Tick layer (index 1) now has", editor.ruleCount(tickLayer), "rules;",
                        "the OUTPUT category has three (Rigid stamp / Bending stamp / Jointed stamp)")
            console.log("-> hover an Output row to grab the ⠿ handle, drag it over another,",
                        "and release to reorder. The row labels make the move visible.")
            console.log("Window stays open ~100s (Ctrl-C to quit).")

            wait(100000)
        }
    }
}
