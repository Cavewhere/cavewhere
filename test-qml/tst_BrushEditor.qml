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

            // The TreeView instantiates rule rows lazily once its layers expand.
            tryVerify(() => findChild(rootId, "ruleCheckBox_1_0") !== null, 2000,
                      "the first tick-layer rule checkbox exists")
            // Re-fetch before each use: TreeView pools/reuses delegates, so a
            // captured handle could go stale.
            verify(findChild(rootId, "ruleCheckBox_1_0").checked, "rule starts enabled")
            verify(!editor.dirty, "freshly loaded editor is clean")

            mouseClick(findChild(rootId, "ruleCheckBox_1_0"))

            tryVerify(() => editor.dirty, 2000, "toggling a rule dirties the working copy")
            verify(!editor.ruleEnabled(1, 0), "the rule is now disabled on the working copy")

            const discardButton = findChild(rootId, "brushEditorDiscardButton")
            verify(discardButton !== null && discardButton.enabled, "Discard is enabled when dirty")
            mouseClick(discardButton)

            tryVerify(() => !editor.dirty, 2000, "Discard clears the dirty state")
            verify(editor.ruleEnabled(1, 0), "Discard restores the rule")
        }

        function test_removeRuleViaButtonMarksDirtyThenDiscardReverts() {
            const editor = openEditorOnFloorStep()

            tryVerify(() => findChild(rootId, "removeRuleButton_1_0") !== null, 2000,
                      "the first tick-layer rule's remove button exists")
            const before = editor.ruleCount(1)
            verify(!editor.dirty, "freshly loaded editor is clean")

            mouseClick(findChild(rootId, "removeRuleButton_1_0"))

            tryVerify(() => editor.ruleCount(1) === before - 1, 2000, "removing a rule drops one")
            verify(editor.dirty, "removing a rule dirties the working copy")

            const discardButton = findChild(rootId, "brushEditorDiscardButton")
            mouseClick(discardButton)

            tryVerify(() => editor.ruleCount(1) === before, 2000, "Discard restores the removed rule")
            verify(!editor.dirty, "Discard clears the dirty state")
        }

        function test_addRuleInsertsTreeRow() {
            const editor = openEditorOnFloorStep()

            const tree = findChild(rootId, "brushStructureTree")
            verify(tree !== null, "the structure tree exists")
            tryVerify(() => tree.rows > 0, 2000, "the tree has expanded rows")
            const beforeRows = tree.rows
            const before = editor.ruleCount(1)
            const groups = editor.availableRuleGroups()
            verify(groups.length > 0, "the registry exposes rules to add")

            // Drive the editor directly (the popup is exercised separately). The
            // QML-relevant assertion is that the model's rowsInserted grows the
            // view's expanded row set (checking tree.rows, not a delegate that may
            // sit below the clipped viewport).
            editor.addRule(1, groups[0].rules[0].name)

            compare(editor.ruleCount(1), before + 1, "the rule is appended")
            tryVerify(() => tree.rows === beforeRows + 1, 2000,
                      "the inserted rule adds a row to the tree")
            verify(editor.dirty, "adding a rule dirties the working copy")

            editor.discard()
        }

        // The add-rule popup reparents to a popup layer outside rootId's subtree,
        // so reach its rows by walking up to the item-tree root.
        function sceneRoot() {
            let item = rootId
            while (item.parent) {
                item = item.parent
            }
            return item
        }

        function test_addRuleViaPopupPicker() {
            const editor = openEditorOnFloorStep()

            const before = editor.ruleCount(1)
            mouseClick(findChild(rootId, "addRuleButton_1"))

            // Offset stroke is the only Stroke-stage rule, so availableRuleGroups()
            // (sorted by stage) always lists it first; Repeater instantiates every
            // row eagerly, so findChild reaches it regardless of scroll position.
            const itemName = "addRuleItem_1_Offset stroke"
            tryVerify(() => findChild(sceneRoot(), itemName) !== null, 2000,
                      "the picker lists rules to add")
            mouseClick(findChild(sceneRoot(), itemName))

            tryVerify(() => editor.ruleCount(1) === before + 1, 2000,
                      "picking a rule appends it to the layer")
            compare(editor.ruleName(1, before), "Offset stroke", "the chosen rule is added")
            verify(editor.dirty, "adding a rule dirties the working copy")

            editor.discard()
        }
    }
}
