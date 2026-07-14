import QtQuick
import QtTest
import QmlTestRecorder
import cavewherelib

Item {
    id: rootId
    objectName: "rootId"

    // Tall enough that the brush editor's structure tree shows every layer/rule
    // row without scrolling: TreeView pools off-screen delegates, so a clipped row
    // can't be found or clicked, and leftover scroll state would leak across tests.
    width: 700
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
        name: "BrushEditor"
        when: windowShown

        // Functions in a TestCase share one Sketch; clear strokes between them so
        // each starts from an empty sketch instead of relying on the continuation
        // finder to merge a leaked stroke (which would mask order dependence).
        function cleanup() {
            // Discard and close the editor so a test that fails mid-way (leaving a
            // dirty, open panel) can't poison the next one.
            const panel = findChild(sketchItemId, "brushEditorPanel")
            if (panel !== null) {
                const editor = findChild(panel, "brushEditor")
                if (editor !== null) {
                    editor.discard()
                }
                panel.open = false
            }
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

        // Move the mouse over a rule row so its hover-gated affordances (remove
        // button, drag handle) appear, then return the row's checkbox (always
        // present) for reference.
        function hoverRuleRow(layerIndex, ruleIndex) {
            const checkBox = findChild(rootId, "ruleCheckBox_" + layerIndex + "_" + ruleIndex)
            verify(checkBox !== null, "rule row checkbox exists")
            mouseMove(checkBox, checkBox.width / 2, checkBox.height / 2)
            return checkBox
        }

        // Move the mouse over a layer row so its hover-gated controls (move
        // up/down, remove) appear. The add-rule "+" button is always present on a
        // layer row, so hovering it triggers the row's HoverHandler.
        function hoverLayerRow(layerIndex) {
            const addButton = findChild(rootId, "addRuleButton_" + layerIndex)
            verify(addButton !== null, "layer row add-rule button exists")
            mouseMove(addButton, addButton.width / 2, addButton.height / 2)
            return addButton
        }

        function test_removeRuleViaButtonMarksDirtyThenDiscardReverts() {
            const editor = openEditorOnFloorStep()

            tryVerify(() => findChild(rootId, "removeRuleButton_1_0") !== null, 2000,
                      "the first tick-layer rule's remove button exists")
            const before = editor.ruleCount(1)
            verify(!editor.dirty, "freshly loaded editor is clean")

            // The remove button is always laid out (so the row height is fixed)
            // but only becomes visible/clickable on hover.
            verify(findChild(rootId, "removeRuleButton_1_0").opacity === 0,
                   "remove button hidden without hover")
            hoverRuleRow(1, 0)
            tryVerify(() => findChild(rootId, "removeRuleButton_1_0").enabled, 2000,
                      "the remove button enables on hover")
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

            // Drive the editor directly (the popup is exercised separately). Add a
            // rule into a stage the tick layer already has (Output holds Rigid
            // stamp), so it joins the existing group and adds exactly one row. A
            // brand-new stage would add two rows (group + its rule); that path is
            // covered in the C++ model test. The QML-relevant assertion is that the
            // model's rowsInserted grows the view's expanded row set (tree.rows,
            // not a delegate that may sit below the clipped viewport).
            editor.addRule(1, "Bending stamp")

            compare(editor.ruleCount(1), before + 1, "the rule is appended")
            tryVerify(() => tree.rows === beforeRows + 1, 2000,
                      "the inserted rule adds a row to the tree")
            verify(editor.dirty, "adding a rule dirties the working copy")

            editor.discard()
        }

        // Category group nodes are real tree rows now; find one by layer + label
        // and return its rendered (uppercased) text, or null if it isn't realized
        // yet. Null-safe in one poll: TreeView pools delegates, so a two-step
        // "find then read" can race a recycle.
        function categoryNodeText(layerIndex, label) {
            const node = findChild(rootId, "brushCategory_" + layerIndex + "_" + label)
            return node === null ? null : node.text
        }

        function test_rulesGroupedByCategoryNodes() {
            const editor = openEditorOnFloorStep()

            // floor-step's tick layer runs Uniform spacing (Placement) ->
            // Align to tangent (Orientation) -> Rigid stamp (Output): three
            // single-rule stages, so the layer holds three category group nodes.
            tryVerify(() => categoryNodeText(1, "Placement") === "PLACEMENT", 2000,
                      "the Placement group header renders")
            tryVerify(() => categoryNodeText(1, "Orientation") === "ORIENTATION", 2000,
                      "the Orientation group header renders")
            tryVerify(() => categoryNodeText(1, "Output") === "OUTPUT", 2000,
                      "the Output group header renders")

            editor.discard()
        }

        function test_dragHandleHoverGatedAndHiddenWhenAloneInCategory() {
            const editor = openEditorOnFloorStep()

            tryVerify(() => findChild(rootId, "dragHandle_1_0") !== null, 2000,
                      "the first tick-rule's drag handle exists")
            // Always laid out, but painted (opacity) only when reorderable+hovered.
            verify(findChild(rootId, "dragHandle_1_0").opacity === 0, "handle hidden without hover")

            // Each shipped tick rule is alone in its category. Hovering shows the
            // remove button (proving hover works) but NOT the drag handle, since
            // there's nothing to reorder. The positive gate (category >= 2 rules
            // -> handle on hover) is covered deterministically in the C++ model
            // test; re-hovering a delegate rebuilt mid-test is unreliable offscreen.
            hoverRuleRow(1, 0)
            tryVerify(() => findChild(rootId, "removeRuleButton_1_0").enabled, 2000,
                      "hover registered (remove button enables)")
            verify(findChild(rootId, "dragHandle_1_0").opacity === 0,
                   "no handle when the rule is alone in its category")

            editor.discard()
        }

        // The live rule-row delegate for a given tick-layer rule index, read by
        // walking the view's rows (authoritative — findChild-by-objectName can
        // return a pooled/stale delegate). null if it isn't currently realized.
        function tickRuleDelegate(ruleIndex) {
            const tree = findChild(rootId, "brushStructureTree")
            for (let r = 0; r < tree.rows; r++) {
                const item = tree.itemAtCell(Qt.point(0, r))
                if (item !== null && item.isRule
                        && item.layerIndex === 1 && item.ruleIndex === ruleIndex) {
                    return item
                }
            }
            return null
        }

        // EVERY rule delegate of a layer that is actually instantiated and visible,
        // found by walking the content item's item tree — NOT by itemAtCell, which
        // maps a logical row back to the model and so can never surface a ghost or
        // duplicated delegate (one not tied to a logical cell). Sorted by real y in
        // the content item, so it reflects the painted order. Returns
        // [{y, ruleIndex, label}, ...] top-to-bottom. This is the probe that catches
        // a stranded/duplicated row after a structural edit.
        function visualRuleRows(layerIndex) {
            const tree = findChild(rootId, "brushStructureTree")
            let rows = []
            function walk(item) {
                const kids = item.children
                for (let i = 0; i < kids.length; i++) {
                    const c = kids[i]
                    if (c.isRule === true && c.layerIndex === layerIndex
                            && c.visible && c.height > 0) {
                        const p = c.mapToItem(tree.contentItem, 0, 0)
                        rows.push({ y: p.y, ruleIndex: c.ruleIndex, label: c.label })
                    }
                    walk(c)
                }
            }
            walk(tree.contentItem)
            rows.sort((a, b) => a.y - b.y)
            return rows
        }

        // The painted rule labels of a layer, top-to-bottom.
        function visualRuleLabels(layerIndex) {
            return visualRuleRows(layerIndex).map(r => r.label)
        }

        // How many category group nodes for the given layer carry this label —
        // used to prove a reorder doesn't split or duplicate a group.
        function categoryNodeCount(layerIndex, label) {
            const tree = findChild(rootId, "brushStructureTree")
            let n = 0
            for (let r = 0; r < tree.rows; r++) {
                const item = tree.itemAtCell(Qt.point(0, r))
                if (item !== null && item.isCategory
                        && item.layerIndex === layerIndex && item.category === label) {
                    n += 1
                }
            }
            return n
        }

        // Reordering a rule within its category group reorders the rendered rule
        // rows while leaving the group node itself structural and singular. With
        // category groups as their own fixed-height nodes, the old "header smuggled
        // into the first rule" variable-height symptom is gone — so a stale
        // TableView height cache can no longer strand a header on the wrong row.
        function test_reorderWithinCategoryReordersRowsKeepingOneGroup() {
            const editor = openEditorOnFloorStep()

            // tick layer (1) ships [Uniform(Placement), Align(Orientation),
            // Rigid(Output)]; add two more Output rules so the Output group has
            // three reorderable rows (Rigid, Bending, Jointed at flat 2,3,4).
            editor.addRule(1, "Bending stamp")
            editor.addRule(1, "Jointed stamp")
            compare(editor.ruleCount(1), 5, "tick layer has five rules")
            compare(editor.ruleName(1, 2), "Rigid stamp")
            compare(editor.ruleName(1, 4), "Jointed stamp")

            tryVerify(() => tickRuleDelegate(2) !== null, 2000, "rule rows materialize")
            compare(categoryNodeCount(1, "Output"), 1, "one Output group before the move")

            // Move Jointed stamp (rule 4) up above Rigid stamp (rule 2).
            editor.moveRule(1, 4, 2)

            // --- model data ---
            compare(editor.ruleName(1, 2), "Jointed stamp", "data: Jointed now leads Output")
            compare(editor.ruleName(1, 3), "Rigid stamp")
            compare(editor.ruleName(1, 4), "Bending stamp")

            // --- rendered rule rows track the move (read the live delegates) ---
            tryVerify(() => {
                const it = tickRuleDelegate(2)
                return it !== null && it.label === "Jointed stamp"
            }, 2000, "view: the moved Jointed stamp now sits at rule index 2")
            compare(tickRuleDelegate(3).label, "Rigid stamp")
            compare(tickRuleDelegate(4).label, "Bending stamp")

            // The group is structural: still exactly one Output node after the move.
            compare(categoryNodeCount(1, "Output"), 1, "still one Output group after the move")

            // Fixed-height rows: every rule row lays out the same height, so a
            // stale height cache can't strand a header on the wrong row.
            compare(tickRuleDelegate(2).height, tickRuleDelegate(3).height,
                    "reordered rule rows keep a uniform height")

            editor.discard()
        }

        // Reproduces the reported bug: after moving Jointed to the top of the
        // Output group the MODEL is correct, but the TreeView paints the row in the
        // wrong place. Checks the painted (y-sorted) order, not just the model.
        function test_reorderUpdatesPaintedRowOrder() {
            const editor = openEditorOnFloorStep()

            editor.addRule(1, "Bending stamp")
            editor.addRule(1, "Jointed stamp")
            // tick layer rules: Uniform, Align, Rigid, Bending, Jointed (flat 0..4).
            tryVerify(() => tickRuleDelegate(4) !== null, 2000, "rule rows materialize")
            compare(visualRuleLabels(1),
                    ["Uniform spacing", "Align to tangent", "Rigid stamp",
                     "Bending stamp", "Jointed stamp"],
                    "painted order matches the model before the move")

            // Move Jointed (4) to the top of the Output group (flat 2).
            editor.moveRule(1, 4, 2)

            // Model is correct (covered elsewhere); here assert the PAINTED order.
            tryVerify(() => visualRuleLabels(1)[2] === "Jointed stamp", 2000,
                      "the painted third rule row is the moved Jointed stamp")
            compare(visualRuleLabels(1),
                    ["Uniform spacing", "Align to tangent", "Jointed stamp",
                     "Rigid stamp", "Bending stamp"],
                    "painted order tracks the move")

            // No stranded/duplicated delegate: exactly five rule rows are painted
            // and their rule indices are the distinct set 0..4.
            const painted = visualRuleRows(1)
            compare(painted.length, 5, "exactly five rule rows are painted")
            const indices = painted.map(r => r.ruleIndex).sort((a, b) => a - b)
            compare(indices, [0, 1, 2, 3, 4], "each painted row maps to a distinct rule")

            editor.discard()
        }

        // Drive the real drag-to-reorder: grab a rule's ⠿ handle and drag the
        // pointer over another rule's top half, then release. Unlike the synthetic
        // moveRule tests, this exercises the QML drop-slot hit-testing — the path
        // the user actually hits. Coordinates are taken in the TreeView's space so
        // the move spans rows.
        function dragRuleAbove(fromRuleIndex, targetRuleIndex) {
            const tree = findChild(rootId, "brushStructureTree")
            tryVerify(() => tickRuleDelegate(fromRuleIndex) !== null, 2000,
                      "source rule row realized")
            tryVerify(() => tickRuleDelegate(targetRuleIndex) !== null, 2000,
                      "target rule row realized")
            const sourceRow = tickRuleDelegate(fromRuleIndex)
            const handle = findChild(sourceRow, "dragHandle_1_" + fromRuleIndex)
            verify(handle !== null, "source rule has a drag handle")

            const start = handle.mapToItem(tree, handle.width / 2, handle.height / 2)
            // Aim at the target row's TOP quarter so the drop lands above it.
            const targetRow = tickRuleDelegate(targetRuleIndex)
            const drop = targetRow.mapToItem(tree, targetRow.width / 2, targetRow.height * 0.25)

            mousePress(tree, start.x, start.y, Qt.LeftButton)
            // First nudge past the drag threshold, then walk to the target in a
            // couple of steps so the handler tracks the move.
            mouseMove(tree, start.x, start.y - 12)
            mouseMove(tree, drop.x, Math.round((start.y + drop.y) / 2))
            mouseMove(tree, drop.x, drop.y)
            mouseMove(tree, drop.x, drop.y)
            mouseRelease(tree, drop.x, drop.y, Qt.LeftButton)
        }

        function test_dragReordersRuleWithinGroup() {
            const editor = openEditorOnFloorStep()

            // Output group: Rigid(2), Bending(3), Jointed(4).
            editor.addRule(1, "Bending stamp")
            editor.addRule(1, "Jointed stamp")
            compare(editor.ruleName(1, 2), "Rigid stamp")
            compare(editor.ruleName(1, 3), "Bending stamp")
            compare(editor.ruleName(1, 4), "Jointed stamp")

            // Drag Jointed (4) up above Bending (3): expect Rigid, Jointed, Bending.
            dragRuleAbove(4, 3)

            tryVerify(() => editor.ruleName(1, 3) === "Jointed stamp", 2000,
                      "Jointed lands above Bending")
            compare(editor.ruleName(1, 2), "Rigid stamp", "Rigid stays first in Output")
            compare(editor.ruleName(1, 3), "Jointed stamp")
            compare(editor.ruleName(1, 4), "Bending stamp")

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
                      "picking a rule adds it to the layer")
            // Offset stroke is a Stroke-stage rule, so it sorts to the front of
            // the layer's stack regardless of where the existing rules sit.
            compare(editor.ruleName(1, 0), "Offset stroke", "the chosen rule lands in its stage block")
            verify(editor.dirty, "adding a rule dirties the working copy")

            editor.discard()
        }

        // A malformed rule stack surfaces an inline error icon on the offending
        // row, and the icon clears once the stack is well-formed again. This is
        // model-driven delegate visibility, so it's reliable offscreen.
        function test_inlineWarningIconShowsForDeadTerminalAndClearsAfterFix() {
            const editor = openEditorOnFloorStep()

            // tick layer ships Uniform(0), Align(1), Rigid stamp(2 — the Output
            // terminal). Adding a second terminal (Trace) joins the Output stage,
            // so the extra terminal is dead (fatal) and its row shows the icon.
            editor.addRule(1, "Trace")
            compare(editor.ruleName(1, 3), "Trace", "the second terminal sits in the Output stage")

            tryVerify(() => {
                const icon = findChild(rootId, "ruleErrorIcon_1_3")
                return icon !== null && icon.visible
            }, 2000, "the dead terminal row shows an inline error icon")

            // The well-formed terminal above it stays clean.
            const goodIcon = findChild(rootId, "ruleErrorIcon_1_2")
            verify(goodIcon !== null && !goodIcon.visible,
                   "the in-order terminal row shows no icon")

            // Removing the dead terminal makes the stack well-formed; the remaining
            // terminal's row has no icon.
            editor.removeRule(1, 3)
            tryVerify(() => {
                const icon = findChild(rootId, "ruleErrorIcon_1_2")
                return icon !== null && !icon.visible
            }, 2000, "fixing the stack clears the inline error icon")

            editor.discard()
        }

        // Named test_layer* (not test_addLayer*) so it sorts AFTER
        // test_addRuleInsertsTreeRow: that test asserts an exact +1 row delta and
        // must run first on a pristine TreeView. Asserts row existence (findChild),
        // not an exact count, so it is itself robust to expansion timing.
        function test_layerAddAppendsRow() {
            const editor = openEditorOnFloorStep()

            compare(editor.layerCount(), 2, "floor-step starts with two layers")
            verify(findChild(rootId, "addRuleButton_2") === null, "no third layer row yet")

            mouseClick(findChild(rootId, "addLayerButton"))

            tryVerify(() => editor.layerCount() === 3, 2000, "a layer is appended")
            // The appended layer materializes its own row (its add-rule button appears).
            tryVerify(() => findChild(rootId, "addRuleButton_2") !== null, 2000,
                      "the appended layer shows a tree row")
            verify(editor.dirty, "adding a layer dirties the working copy")

            editor.discard()
            tryVerify(() => editor.layerCount() === 2, 2000, "Discard restores the layer count")
            tryVerify(() => findChild(rootId, "addRuleButton_2") === null, 2000,
                      "Discard removes the appended layer row")
        }

        function test_layerRemoveViaButtonMarksDirty() {
            const editor = openEditorOnFloorStep()
            compare(editor.layerCount(), 2, "floor-step starts with two layers")

            tryVerify(() => findChild(rootId, "removeLayerButton_0") !== null, 2000,
                      "the line layer's remove button exists")
            verify(findChild(rootId, "removeLayerButton_0").opacity === 0,
                   "remove button hidden without hover")
            hoverLayerRow(0)
            tryVerify(() => findChild(rootId, "removeLayerButton_0").enabled, 2000,
                      "the remove button enables on hover")
            mouseClick(findChild(rootId, "removeLayerButton_0"))

            tryVerify(() => editor.layerCount() === 1, 2000, "removing drops the layer")
            verify(editor.dirty, "removing a layer dirties the working copy")

            editor.discard()
            tryVerify(() => editor.layerCount() === 2, 2000, "Discard restores the layer")
        }

        // Stable layer ids let add/remove emit row signals instead of a model
        // reset, so the pre-existing layer 0 delegate is NOT rebuilt. Its grip
        // visibility must still track the layer count — proof the binding rides
        // the layerCount NOTIFY property, not a delegate recreation. Before the
        // NOTIFY property this binding went stale (the surviving delegate kept its
        // old value across the 2->1 / 1->2 threshold).
        function test_layerGripReactsToCountWithoutReset() {
            const editor = openEditorOnFloorStep()
            compare(editor.layerCount(), 2, "floor-step starts with two layers")

            const grip0 = () => findChild(rootId, "layerDragHandle_0")
            hoverLayerRow(0)
            tryVerify(() => grip0() !== null && grip0().opacity === 1, 2000,
                      "layer 0 grip is visible on hover with two layers")

            // Drop to one layer: layer 0 survives in place (no reset), so its grip
            // can only hide if the binding re-evaluated against the new count.
            hoverLayerRow(1)
            tryVerify(() => findChild(rootId, "removeLayerButton_1").enabled, 2000,
                      "the tick layer's remove button enables on hover")
            mouseClick(findChild(rootId, "removeLayerButton_1"))
            tryVerify(() => editor.layerCount() === 1, 2000, "removing drops to one layer")
            hoverLayerRow(0)
            tryVerify(() => grip0().opacity === 0, 2000,
                      "layer 0 grip hides at one layer (not reorderable)")

            // Back to two layers: the same surviving delegate's grip reappears.
            mouseClick(findChild(rootId, "addLayerButton"))
            tryVerify(() => editor.layerCount() === 2, 2000, "a layer is appended")
            hoverLayerRow(0)
            tryVerify(() => grip0().opacity === 1, 2000,
                      "layer 0 grip is visible again with two layers")

            editor.discard()
        }

        // The live layer-header delegate for a layer index, found by walking the
        // view's rows. null if it isn't currently realized.
        function layerDelegate(layerIndex) {
            const tree = findChild(rootId, "brushStructureTree")
            for (let r = 0; r < tree.rows; r++) {
                const item = tree.itemAtCell(Qt.point(0, r))
                if (item !== null && item.isLayer && item.layerIndex === layerIndex) {
                    return item
                }
            }
            return null
        }

        // Drive the real layer drag-to-reorder: grab a layer's ⠿ handle and drag
        // the pointer over another layer header's top quarter, then release —
        // exercising the QML drop-slot hit-testing, like dragRuleAbove does for
        // rules.
        function dragLayerAbove(fromLayerIndex, targetLayerIndex) {
            const tree = findChild(rootId, "brushStructureTree")
            tryVerify(() => layerDelegate(fromLayerIndex) !== null, 2000, "source layer realized")
            tryVerify(() => layerDelegate(targetLayerIndex) !== null, 2000, "target layer realized")
            const sourceRow = layerDelegate(fromLayerIndex)
            const handle = findChild(sourceRow, "layerDragHandle_" + fromLayerIndex)
            verify(handle !== null, "source layer has a drag handle")

            const start = handle.mapToItem(tree, handle.width / 2, handle.height / 2)
            const targetRow = layerDelegate(targetLayerIndex)
            const drop = targetRow.mapToItem(tree, targetRow.width / 2, targetRow.height * 0.25)

            mousePress(tree, start.x, start.y, Qt.LeftButton)
            mouseMove(tree, start.x, start.y - 12)
            mouseMove(tree, drop.x, Math.round((start.y + drop.y) / 2))
            mouseMove(tree, drop.x, drop.y)
            mouseMove(tree, drop.x, drop.y)
            mouseRelease(tree, drop.x, drop.y, Qt.LeftButton)
        }

        function test_layerDragReordersPaintOrder() {
            const editor = openEditorOnFloorStep()

            // floor-step: layer 0 = line (empty label), layer 1 = tick glyph.
            const tickLabel = editor.layerLabel(1)
            verify(editor.layerLabel(0) === "", "layer 0 is the line layer")
            verify(tickLabel !== "", "layer 1 is the tick glyph layer")

            // Drag the tick layer (1) up above the line layer (0).
            dragLayerAbove(1, 0)

            tryVerify(() => editor.layerLabel(0) === tickLabel, 2000,
                      "the tick layer dragged to the top")
            compare(editor.layerLabel(1), "", "the line layer moved down")
            verify(editor.dirty, "reordering dirties the working copy")

            editor.discard()
        }

        // A layer's glyph picker swaps or clears the layer's glyph. The popup's
        // always-present "Line layer (no glyph)" header row clears it
        // deterministically (no scrolling to an off-screen glyph row needed).
        function test_layerGlyphPickerClearsGlyphToLineLayer() {
            const editor = openEditorOnFloorStep()

            // Layer 1 is the tick (stamp) layer; it ships naming a glyph.
            verify(editor.layerLabel(1) !== "", "the tick layer starts with a glyph")

            tryVerify(() => findChild(rootId, "layerGlyphPicker_1") !== null, 2000,
                      "the tick layer's glyph picker exists")
            mouseClick(findChild(rootId, "layerGlyphPicker_1"))

            // The clear row is the popup's header (always present); the popup
            // reparents to a popup layer, so reach it from the scene root.
            tryVerify(() => findChild(sceneRoot(), "layerGlyphClearRow") !== null, 2000,
                      "the glyph picker popup opens with a clear row")
            mouseClick(findChild(sceneRoot(), "layerGlyphClearRow"))

            tryVerify(() => editor.layerLabel(1) === "", 2000,
                      "clearing the glyph turns the layer into a line layer")
            verify(editor.dirty, "changing a layer's glyph dirties the working copy")

            editor.discard()
            tryVerify(() => editor.layerLabel(1) !== "", 2000, "Discard restores the glyph")
        }

        function test_symbologyErrorEnumExposedToQml() {
            // The validation codes the model puts in ruleErrorCodes are exposed as
            // a named QML enum so a fix-it UI can dispatch on problem kind without
            // comparing against magic numbers.
            compare(SymbologyError.TwoTerminals, 1024, "TwoTerminals code is stable")
            compare(SymbologyError.RulesOutOfOrder, 1032, "RulesOutOfOrder code is stable")
        }
    }
}
