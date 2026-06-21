import QtQuick
import QtTest
import QmlTestRecorder
import cavewherelib

Item {
    id: rootId
    objectName: "rootId"

    width: 700
    height: 400

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
        name: "BrushEditor"
        when: windowShown

        // Functions in a TestCase share one Sketch; clear strokes between them so
        // each starts from an empty sketch instead of relying on the continuation
        // finder to merge a leaked stroke (which would mask order dependence).
        function cleanup() {
            sketchId.clearStrokes()
        }

        function drawStroke(points) {
            mousePress(sketchItemId, points[0].x, points[0].y, Qt.LeftButton)
            for (let i = 1; i < points.length; i++) {
                mouseMove(sketchItemId, points[i].x, points[i].y, 0, Qt.LeftButton)
            }
            const last = points[points.length - 1]
            mouseRelease(sketchItemId, last.x, last.y, Qt.LeftButton)
        }

        // Draw a floor-step stroke and open the editor on it by right-clicking.
        // Returns the BrushEditor controller once the panel is open.
        function openEditorOnFloorStep() {
            const canvas = findChild(sketchItemId, "sketchCanvas")
            verify(canvas !== null, "sketchCanvas should exist")
            tryVerify(() => sketchId.resolvedPalette !== null, 4000,
                      "palette should resolve before drawing")

            canvas.currentBrushName = "floor-step"
            drawStroke([Qt.point(180, 200), Qt.point(280, 200), Qt.point(380, 200)])
            tryVerify(() => sketchId.strokeCount() === 1, 2000, "one stroke drawn")

            // Right-click (tap, no drag) on the stroke -> editBrushRequested.
            mouseClick(sketchItemId, 280, 200, Qt.RightButton)

            const panel = findChild(sketchItemId, "brushEditorPanel")
            verify(panel !== null, "brush editor panel should exist")
            tryVerify(() => panel.visible, 2000, "panel opens on right-click")

            const editor = findChild(panel, "brushEditor")
            verify(editor !== null, "editor controller should exist")
            tryVerify(() => editor.loaded && editor.brushName === "floor-step", 2000,
                      "editor loads the stroke's brush")
            return editor
        }

        function test_rightClickOpensEditorWithStructure() {
            const editor = openEditorOnFloorStep()

            // floor-step: an edge (Trace) layer + a tick-stamp layer.
            compare(editor.layerCount(), 2, "two decoration layers")
            compare(editor.ruleCount(1), 3, "tick layer has three rules")
            compare(editor.ruleName(1, 0), "Uniform spacing")

            // The shipped default is read-only, so the commit is Duplicate & Save
            // and the read-only banner shows.
            verify(!editor.paletteWritable, "shipped default is read-only")
            const banner = findChild(rootId, "brushEditorReadOnlyBanner")
            verify(banner !== null && banner.visible, "read-only banner is shown")
            const applyButton = findChild(rootId, "brushEditorApplyButton")
            compare(applyButton.text, "Duplicate & Save…")

            editor.discard()
        }

        function test_toggleRuleViaCheckboxMarksDirtyThenDiscardReverts() {
            const editor = openEditorOnFloorStep()

            const checkBox = findChild(rootId, "ruleCheckBox_1_0")
            verify(checkBox !== null, "the first tick-layer rule checkbox exists")
            verify(checkBox.checked, "rule starts enabled")
            verify(!editor.dirty, "freshly loaded editor is clean")

            mouseClick(checkBox)

            tryVerify(() => editor.dirty, 2000, "toggling a rule dirties the working copy")
            verify(!editor.ruleEnabled(1, 0), "the rule is now disabled on the working copy")

            const discardButton = findChild(rootId, "brushEditorDiscardButton")
            verify(discardButton !== null && discardButton.enabled, "Discard is enabled when dirty")
            mouseClick(discardButton)

            tryVerify(() => !editor.dirty, 2000, "Discard clears the dirty state")
            verify(editor.ruleEnabled(1, 0), "Discard restores the rule")
        }
    }
}
