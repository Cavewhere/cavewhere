/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// Commit 9, phase 1 — the brush editor "thin loop". A panel docked beside the
// live sketch: it owns a BrushEditor working copy, shows the brush's layer/rule
// structure with enable/disable checkboxes, and feeds a preview snapshot into
// the real SketchCanvas so toggles re-skin the sketch immediately. Discard
// reverts; Apply commits (Duplicate & Save on a read-only palette). Read-only
// structure only — add/remove/reorder and param editing are phase 2.
QQ.Rectangle {
    id: panel

    objectName: "brushEditorPanel"

    // The live canvas the preview is pushed into, the region whose default
    // palette fork-on-save repoints, and the palette a freshly-opened brush is
    // read from (the sketch's resolved palette).
    // External wiring the panel can't function without; the host must supply all
    // three. region may resolve to null (fork-on-save just won't repoint), but it
    // is still explicitly wired.
    required property SketchCanvas previewCanvas
    required property CavingRegion region
    // Named symbologyPalette, not palette: a bare `palette` would shadow
    // QQuickItem.palette (the theming palette).
    required property SymbologyPalette symbologyPalette

    property bool open: false

    readonly property int kPanelWidth: 280

    // Open the panel on the named brush, snapshotting the current resolved
    // palette as the edit target. Assigned imperatively (not bound) so apply()'s
    // fork-on-save can re-target the editor without a binding fighting it.
    function openFor(brushName) {
        editorId.palette = panel.symbologyPalette
        editorId.loadBrushNamed(brushName)
        if (editorId.loaded) {
            panel.open = true
        }
    }

    function _switchPalette(paletteId) {
        if (panel.region === null || panel.previewCanvas === null
                || panel.previewCanvas.sketch === null) {
            return
        }
        panel.region.defaultPaletteId = paletteId
        // After the region repoints, the sketch re-resolves; re-target the editor
        // at the new palette and reload the same brush from it.
        const name = editorId.brushName
        editorId.palette = panel.previewCanvas.sketch.resolvedPalette
        editorId.loadBrushNamed(name)
    }

    width: kPanelWidth
    visible: open && editorId.loaded
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    BrushEditor {
        id: editorId
        objectName: "brushEditor"
        previewCanvas: panel.previewCanvas
        region: panel.region
    }

    SymbologyPaletteListModel {
        id: paletteListModelId
        manager: RootData.paletteManager
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: Theme.sectionSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.flowSpacing

            QC.Label {
                text: "Edit Brush"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMedium
                Layout.fillWidth: true
            }

            QC.ToolButton {
                objectName: "brushEditorCloseButton"
                text: "✕"
                onClicked: {
                    editorId.discard()
                    panel.open = false
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.flowSpacing

            QC.Label {
                text: "Palette:"
                color: Theme.textSubtle
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            QC.ComboBox {
                id: paletteComboId
                objectName: "brushEditorPaletteCombo"
                Layout.fillWidth: true
                model: paletteListModelId
                textRole: "name"
                valueRole: "id"
                currentIndex: editorId.palette !== null ? indexOfValue(editorId.palette.id) : -1
                onActivated: panel._switchPalette(currentValue)
            }
        }

        // Read-only palette → editing a working copy that Duplicate & Save forks.
        QQ.Rectangle {
            Layout.fillWidth: true
            visible: editorId.loaded && !editorId.paletteWritable
            color: Theme.warning
            radius: Theme.floatingWidgetRadius
            implicitHeight: bannerLabelId.implicitHeight + 2 * Theme.flowSpacing

            QC.Label {
                id: bannerLabelId
                objectName: "brushEditorReadOnlyBanner"
                anchors.fill: parent
                anchors.margins: Theme.flowSpacing
                wrapMode: QQ.Text.WordWrap
                text: "⚠ Editing a copy of “" + editorId.brushDisplayName + "” (read-only)"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.flowSpacing

            QC.Button {
                objectName: "addLayerButton"
                text: "＋ Layer"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                onClicked: editorId.addLayer()
            }

            QQ.Item { Layout.fillWidth: true }
        }

        QQ.TreeView {
            id: structureTreeId
            objectName: "brushStructureTree"

            // Active rule-reorder drag (handle-driven). dragLayer == -1 when idle.
            // dragTo is a flat insertion index in [0, ruleCount]; the drop-line and
            // the source row read these. dragCategory confines the drop to the
            // dragged rule's own pipeline stage (the layout re-sorts across stages).
            // interactive is disabled mid-drag so the TableView doesn't scroll out
            // from under the handle (and so the dragged delegate isn't recycled).
            property int dragLayer: -1
            property int dragFrom: -1
            property int dragTo: -1
            property string dragCategory: ""

            // Active layer-reorder drag (handle-driven), parallel to the rule drag
            // above. layerDragFrom == -1 when idle. layerDragTo is an insertion slot
            // in [0, layerCount]. Layers interleave with their subtrees in the
            // flattened view, so a per-row top/bottom edge can't mark a between-layer
            // gap; layerDropLineY is the content-y of a single overlay indicator
            // positioned at the target boundary instead.
            property int layerDragFrom: -1
            property int layerDragTo: -1
            property real layerDropLineY: 0

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            interactive: dragLayer < 0 && layerDragFrom < 0
            model: editorId.structureModel

            // Content-y of the boundary for a layer insertion slot: the top of slot's
            // header row, or the bottom of the last row for the past-the-end slot.
            function layerBoundaryY(slot) {
                const count = editorId.layerCount()
                if (slot >= count) {
                    const last = structureTreeId.itemAtCell(Qt.point(0, structureTreeId.rows - 1))
                    return last === null ? 0
                        : last.mapToItem(structureTreeId.contentItem, 0, 0).y + last.height
                }
                for (let r = 0; r < structureTreeId.rows; r++) {
                    const it = structureTreeId.itemAtCell(Qt.point(0, r))
                    if (it !== null && it.isLayer && it.layerIndex === slot) {
                        return it.mapToItem(structureTreeId.contentItem, 0, 0).y
                    }
                }
                return 0
            }
            // Pooling strands a removed row's delegate (a structural edit on a
            // nested parent can leave the recycled item painted at its old spot —
            // e.g. a duplicate row after a reorder's remove+insert). This is a
            // genuine Qt Metal scene-graph pooling bug, NOT the old positional
            // internalId: it still reproduces under interactive Metal drag after the
            // structure model's internalId was made fully position-free (stable layer
            // ids + stage-keyed categories), so don't retry reuseItems:true on that
            // basis. (Offscreen never reproduces it.) The structure is tiny and
            // always fully expanded, so destroying delegates instead of reusing them
            // costs nothing and keeps the view honest.
            reuseItems: false

            // Layers, their category groups, and the rules are always shown
            // expanded (mirrors the phase-1 always-open tree); re-expand whenever
            // the model resets on a brush load or discard.
            QQ.Component.onCompleted: expandRecursively()

            QQ.Connections {
                target: editorId.structureModel
                // The view rebuilds its rows asynchronously after a reset, so
                // defer the expand until those rows exist.
                function onModelReset() { Qt.callLater(structureTreeId.expandRecursively) }
            }

            // Single overlay drop indicator for a layer reorder, parented into the
            // content so it tracks the rows. One line (not per-row edges) because a
            // layer boundary falls between whole subtrees, not at one row's edge.
            QQ.Rectangle {
                parent: structureTreeId.contentItem
                height: 2
                width: structureTreeId.width
                color: Theme.accent
                y: structureTreeId.layerDropLineY
                z: 10
                visible: structureTreeId.layerDragFrom >= 0 && structureTreeId.layerDragTo >= 0
            }

            // A plain Item delegate (not TreeViewDelegate) driven by TreeView's
            // attached properties: the native style forbids customizing a
            // TreeViewDelegate's contentItem, and a rule row needs a checkbox.
            // Three node kinds share one fixed-height row: a layer, a category
            // group header, or a rule. Category groups are real tree nodes (not a
            // header smuggled into the first rule), so every row is the same height
            // and the TableView's row-height cache has nothing to get wrong.
            delegate: QQ.Item {
                id: treeDelegateId

                // Assigned by TreeView (required properties, not attached props).
                required property int row
                required property int depth
                required property bool expanded
                required property bool hasChildren
                required property bool isTreeNode
                // Model roles.
                required property int layerIndex
                required property int ruleIndex
                required property bool isLayer
                required property bool isCategory
                required property bool ruleEnabled
                required property string label
                required property string category
                required property int categorySize
                required property bool categoryLast
                // Worst severity for this row (CwError.NoError when clean) and the
                // validation messages behind it. Category rows always report
                // NoError, so the indicator never shows on them.
                required property int ruleErrorSeverity
                required property list<string> ruleErrorMessages

                readonly property bool isRule: !isLayer && !isCategory
                readonly property real kIndent: Theme.sectionSpacing
                // Fixed size for the per-row icon buttons (+ / ✕) and the drag-grip
                // slot, so the row height never changes when hover-only controls
                // appear.
                readonly property real kRowButtonSize: Theme.iconSizeSmall
                // This rule row is the one currently being dragged.
                readonly property bool isDragSource: structureTreeId.dragLayer === layerIndex
                                                     && structureTreeId.dragFrom === ruleIndex
                // This layer row is the one currently being dragged (layer reorder).
                readonly property bool isLayerDragSource: isLayer
                                                          && structureTreeId.layerDragFrom === layerIndex

                implicitWidth: structureTreeId.width
                // Every row is the same fixed height — the whole point of making
                // category groups their own nodes.
                implicitHeight: kRowButtonSize + 2 * Theme.tightSpacing

                // Hover drives the per-row affordances (drag handle + remove); a
                // single handler over the whole row so they show together.
                QQ.HoverHandler {
                    id: rowHoverId
                }

                RowLayout {
                    id: contentRowId

                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    // Indent the tree column by the node's depth (layer -> category
                    // -> rule).
                    anchors.leftMargin: treeDelegateId.isTreeNode
                                        ? treeDelegateId.depth * treeDelegateId.kIndent : 0
                    spacing: Theme.tightSpacing

                    // Expand/collapse indicator on tree-column rows that have
                    // children (layers and category groups); a same-width spacer
                    // keeps leaf rule rows aligned.
                    QC.Label {
                        Layout.preferredWidth: Theme.sectionSpacing
                        horizontalAlignment: QQ.Text.AlignHCenter
                        visible: treeDelegateId.isTreeNode
                        text: !treeDelegateId.hasChildren ? ""
                              : treeDelegateId.expanded ? "▾" : "▸"
                        color: Theme.textSubtle
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption

                        QQ.TapHandler {
                            enabled: treeDelegateId.hasChildren
                            onSingleTapped: structureTreeId.toggleExpanded(treeDelegateId.row)
                        }
                    }

                    // --- Layer node: drag grip + glyph/line name + add/remove. ---

                    // Drag-to-reorder grip for the whole layer (paint order == layer
                    // order, so a layer move is brush-wide — no stage confinement
                    // like a rule move). Mirrors the rule grip: always laid out (so
                    // the row height is fixed), painted only when reorderable (2+
                    // layers) and hovered or dragged. structureModel.layerCount is a
                    // NOTIFY property, so this re-evaluates on add/remove without a
                    // model reset (delegates persist across structural layer edits).
                    QC.Label {
                        id: layerDragHandleId
                        objectName: "layerDragHandle_" + treeDelegateId.layerIndex
                        visible: treeDelegateId.isLayer
                        opacity: editorId.structureModel.layerCount >= 2
                                 && (rowHoverId.hovered || treeDelegateId.isLayerDragSource) ? 1 : 0
                        text: "⠿"
                        color: Theme.textSubtle
                        horizontalAlignment: QQ.Text.AlignHCenter
                        verticalAlignment: QQ.Text.AlignVCenter
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        Layout.preferredWidth: treeDelegateId.kRowButtonSize
                        Layout.preferredHeight: treeDelegateId.kRowButtonSize
                        Layout.alignment: Qt.AlignVCenter

                        QQ.DragHandler {
                            id: layerDragHandlerId
                            // Track the pointer and reorder on release (the handle
                            // itself doesn't move). Same grab dance as the rule grip:
                            // take the grab from the Flickable but never hand it back.
                            target: null
                            enabled: editorId.structureModel.layerCount >= 2
                            grabPermissions: QQ.PointerHandler.CanTakeOverFromItems
                                             | QQ.PointerHandler.CanTakeOverFromHandlersOfDifferentType

                            onActiveChanged: {
                                if (active) {
                                    structureTreeId.layerDragFrom = treeDelegateId.layerIndex
                                    structureTreeId.layerDragTo = treeDelegateId.layerIndex
                                    layerDragHandlerId.updateDropTarget()
                                } else {
                                    const from = structureTreeId.layerDragFrom
                                    let to = structureTreeId.layerDragTo
                                    structureTreeId.layerDragFrom = -1
                                    structureTreeId.layerDragTo = -1
                                    structureTreeId.layerDropLineY = 0
                                    // layerDragTo is an insertion slot in [0, count];
                                    // removing the source first shifts a later slot
                                    // down by one. Collapse to a final index.
                                    if (from >= 0 && to >= 0) {
                                        if (to > from) {
                                            to = to - 1
                                        }
                                        if (to !== from) {
                                            editorId.moveLayer(from, to)
                                        }
                                    }
                                }
                            }
                            onCentroidChanged: if (active) { layerDragHandlerId.updateDropTarget() }

                            // Resolve which layer boundary the pointer is over and set
                            // the drop slot + the overlay line's y. A layer header's
                            // top/bottom half picks before/after it; any descendant
                            // row counts as "after its layer".
                            function updateDropTarget() {
                                const tree = structureTreeId
                                const pt = layerDragHandleId.mapToItem(tree.contentItem,
                                                                       centroid.position.x,
                                                                       centroid.position.y)
                                const cell = tree.cellAtPosition(pt.x, pt.y, true)
                                if (cell.y < 0) {
                                    return   // off the table; keep the last slot
                                }
                                const rowItem = tree.itemAtCell(cell)
                                if (rowItem === null) {
                                    return
                                }
                                let slot
                                if (rowItem.isLayer) {
                                    const local = layerDragHandleId.mapToItem(rowItem,
                                                                              centroid.position.x,
                                                                              centroid.position.y)
                                    slot = rowItem.layerIndex + (local.y > rowItem.height / 2 ? 1 : 0)
                                } else {
                                    slot = rowItem.layerIndex + 1
                                }
                                tree.layerDragTo = slot
                                tree.layerDropLineY = tree.layerBoundaryY(slot)
                            }
                        }
                    }

                    QC.Label {
                        visible: treeDelegateId.isLayer
                        text: treeDelegateId.label === "" ? "Line layer" : treeDelegateId.label
                        opacity: treeDelegateId.isLayerDragSource ? 0.4 : 1.0
                        color: Theme.textSubtle
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        Layout.fillWidth: true
                    }

                    // Add a rule to this layer, picked from the registry. The rule
                    // set is static, so the grouped picker data is built once.
                    // Fixed size + zero padding so the row height matches the
                    // other rows (the default ToolButton padding is taller).
                    QC.ToolButton {
                        id: addRuleButtonId
                        visible: treeDelegateId.isLayer
                        objectName: "addRuleButton_" + treeDelegateId.layerIndex
                        text: "+"
                        padding: 0
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeMedium
                        implicitWidth: treeDelegateId.kRowButtonSize
                        implicitHeight: treeDelegateId.kRowButtonSize
                        Layout.preferredWidth: treeDelegateId.kRowButtonSize
                        Layout.preferredHeight: treeDelegateId.kRowButtonSize
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: addRulePopupId.open()

                        AddRulePopup {
                            id: addRulePopupId
                            // Drop down-left: the "+" sits at the panel's right edge,
                            // so align the popup's right edge to the button to keep it
                            // on-screen instead of overflowing to the right.
                            x: addRuleButtonId.width - width
                            y: addRuleButtonId.height
                            layerIndex: treeDelegateId.layerIndex
                            groups: editorId.availableRuleGroups()
                            onRuleChosen: (ruleName) => editorId.addRule(treeDelegateId.layerIndex, ruleName)
                        }
                    }

                    // Remove this whole layer. Hover-revealed like the rule remove
                    // button so an idle row stays uncluttered.
                    QC.ToolButton {
                        visible: treeDelegateId.isLayer
                        opacity: rowHoverId.hovered ? 1 : 0
                        enabled: rowHoverId.hovered
                        objectName: "removeLayerButton_" + treeDelegateId.layerIndex
                        text: "✕"
                        padding: 0
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        implicitWidth: treeDelegateId.kRowButtonSize
                        implicitHeight: treeDelegateId.kRowButtonSize
                        Layout.preferredWidth: treeDelegateId.kRowButtonSize
                        Layout.preferredHeight: treeDelegateId.kRowButtonSize
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: editorId.removeLayer(treeDelegateId.layerIndex)
                    }

                    // --- Category node: the pipeline-stage group header. ---
                    QC.Label {
                        visible: treeDelegateId.isCategory
                        objectName: "brushCategory_" + treeDelegateId.layerIndex
                                    + "_" + treeDelegateId.category
                        text: treeDelegateId.category.toUpperCase()
                        color: Theme.textSubtle
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    // --- Rule node: drag grip + enable checkbox + remove. ---

                    // Drag-to-reorder grip. Only meaningful within a category (the
                    // layout re-sorts across stages), so it appears only when the
                    // rule's category holds 2+ rules — and only on hover, or while
                    // this row is the one being dragged.
                    QC.Label {
                        id: dragHandleId
                        objectName: "dragHandle_" + treeDelegateId.layerIndex
                                    + "_" + treeDelegateId.ruleIndex
                        // Always laid out for rule rows (so checkboxes stay aligned
                        // and the row height is fixed); only painted when the
                        // category is reorderable and the row is hovered or dragged.
                        visible: treeDelegateId.isRule
                        opacity: treeDelegateId.categorySize >= 2
                                 && (rowHoverId.hovered || treeDelegateId.isDragSource) ? 1 : 0
                        text: "⠿"
                        color: Theme.textSubtle
                        horizontalAlignment: QQ.Text.AlignHCenter
                        verticalAlignment: QQ.Text.AlignVCenter
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        Layout.preferredWidth: treeDelegateId.kRowButtonSize
                        Layout.preferredHeight: treeDelegateId.kRowButtonSize
                        Layout.alignment: Qt.AlignVCenter

                        QQ.DragHandler {
                            id: dragHandlerId
                            // We don't move the handle itself; we track the pointer
                            // and reorder on release. Disabled when the grip is
                            // hidden so an invisible (opacity 0) slot can't start a
                            // drag.
                            target: null
                            enabled: treeDelegateId.categorySize >= 2
                            // The TreeView's Flickable scrolls on the same
                            // (vertical) axis we drag, and by default a DragHandler
                            // approves takeover by anything — so the Flickable steals
                            // the grab the instant you move and the reorder never
                            // starts. Allow taking the grab FROM the Flickable, but
                            // refuse giving it back.
                            grabPermissions: QQ.PointerHandler.CanTakeOverFromItems
                                             | QQ.PointerHandler.CanTakeOverFromHandlersOfDifferentType

                            onActiveChanged: {
                                if (active) {
                                    structureTreeId.dragLayer = treeDelegateId.layerIndex
                                    structureTreeId.dragCategory = treeDelegateId.category
                                    structureTreeId.dragFrom = treeDelegateId.ruleIndex
                                    structureTreeId.dragTo = treeDelegateId.ruleIndex
                                    dragHandlerId.updateDropTarget()
                                } else {
                                    const layer = structureTreeId.dragLayer
                                    const from = structureTreeId.dragFrom
                                    const slot = structureTreeId.dragTo
                                    let to = slot
                                    // dragTo is an insertion slot in [0, count];
                                    // collapse to a final index for moveRule.
                                    if (to > from) {
                                        to = to - 1
                                    }
                                    structureTreeId.dragLayer = -1
                                    structureTreeId.dragFrom = -1
                                    structureTreeId.dragTo = -1
                                    structureTreeId.dragCategory = ""
                                    const willMove = layer >= 0 && to >= 0 && to !== from
                                    if (willMove) {
                                        editorId.moveRule(layer, from, to)
                                    }
                                }
                            }
                            onCentroidChanged: if (active) { dragHandlerId.updateDropTarget() }

                            // Resolve which rule row the pointer is over and set the
                            // drop slot. We can't rely on a synthetic Drag + DropArea
                            // (internal drag events don't reach the DropAreas here),
                            // so hit-test the TableView directly: cellAtPosition wants
                            // contentItem coordinates, and modelIndex maps the cell
                            // back to a rule.
                            function updateDropTarget() {
                                const tree = structureTreeId
                                const pt = dragHandleId.mapToItem(tree.contentItem,
                                                                  centroid.position.x,
                                                                  centroid.position.y)
                                const cell = tree.cellAtPosition(pt.x, pt.y, true)
                                if (cell.y < 0) {
                                    return   // off the table; keep the last slot
                                }
                                const model = editorId.structureModel
                                const idx = tree.modelIndex(cell)
                                const layer = model.layerIndexOf(idx)
                                const rule = model.ruleIndexOf(idx)
                                const ruleCat = rule >= 0 ? model.ruleCategory(layer, rule) : ""
                                // Confine drops to the dragged rule's own category
                                // (the layout re-sorts across stages).
                                if (layer !== tree.dragLayer || rule < 0
                                        || ruleCat !== tree.dragCategory) {
                                    return
                                }
                                const rowItem = tree.itemAtCell(cell)
                                if (rowItem === null) {
                                    return
                                }
                                // Insert above when in the row's top half, below in
                                // the bottom half -> an insertion slot.
                                const local = dragHandleId.mapToItem(rowItem,
                                                                     centroid.position.x,
                                                                     centroid.position.y)
                                const bottomHalf = local.y > rowItem.height / 2
                                tree.dragTo = rule + (bottomHalf ? 1 : 0)
                            }
                        }
                    }

                    QC.CheckBox {
                        visible: treeDelegateId.isRule
                        objectName: "ruleCheckBox_" + treeDelegateId.layerIndex
                                    + "_" + treeDelegateId.ruleIndex
                        text: treeDelegateId.label
                        checked: treeDelegateId.ruleEnabled
                        opacity: treeDelegateId.isDragSource ? 0.4 : 1.0
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        Layout.fillWidth: true

                        // setRuleEnabled drives the model back (dataChanged), so
                        // rebind checked to the authoritative value the click broke.
                        onToggled: {
                            editorId.setRuleEnabled(treeDelegateId.layerIndex,
                                                    treeDelegateId.ruleIndex, checked)
                            checked = Qt.binding(() => treeDelegateId.ruleEnabled)
                        }
                    }

                    // Always laid out for rule rows (so the row height is fixed);
                    // shown and clickable only on hover or while dragging. Fixed
                    // size + zero padding to match the row height.
                    QC.ToolButton {
                        visible: treeDelegateId.isRule
                        opacity: rowHoverId.hovered || treeDelegateId.isDragSource ? 1 : 0
                        enabled: rowHoverId.hovered || treeDelegateId.isDragSource
                        objectName: "removeRuleButton_" + treeDelegateId.layerIndex
                                    + "_" + treeDelegateId.ruleIndex
                        text: "✕"
                        padding: 0
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        implicitWidth: treeDelegateId.kRowButtonSize
                        implicitHeight: treeDelegateId.kRowButtonSize
                        Layout.preferredWidth: treeDelegateId.kRowButtonSize
                        Layout.preferredHeight: treeDelegateId.kRowButtonSize
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: editorId.removeRule(treeDelegateId.layerIndex,
                                                       treeDelegateId.ruleIndex)
                    }

                    // Inline validation indicator: a warning/error icon (with the
                    // messages as a tooltip) on a layer or rule row whose stack is
                    // malformed. Fixed size so the row height never changes; shown
                    // only when severity rises above NoError (no clean-state icon).
                    QQ.Image {
                        id: errorIconId
                        objectName: "ruleErrorIcon_" + treeDelegateId.layerIndex
                                    + "_" + treeDelegateId.ruleIndex
                        visible: treeDelegateId.ruleErrorSeverity !== CwError.NoError
                        source: treeDelegateId.ruleErrorSeverity === CwError.Fatal
                                ? "qrc:icons/svg/stopSignError.svg"
                                : "qrc:icons/svg/warning.svg"
                        sourceSize.width: treeDelegateId.kRowButtonSize
                        sourceSize.height: treeDelegateId.kRowButtonSize
                        Layout.preferredWidth: treeDelegateId.kRowButtonSize
                        Layout.preferredHeight: treeDelegateId.kRowButtonSize
                        Layout.alignment: Qt.AlignVCenter

                        QQ.HoverHandler {
                            id: errorHoverId
                        }

                        QC.ToolTip {
                            visible: errorIconId.visible && errorHoverId.hovered
                            text: treeDelegateId.ruleErrorMessages.join("\n")
                            delay: Theme.toolTipDelay
                        }
                    }
                }

                // Drop-line between rule rows during a reorder. Rows are fixed
                // height with the content vertically centered, so the row's top
                // edge (y = 0) is the gap above it and its bottom edge the gap
                // below.
                QQ.Rectangle {
                    height: 2
                    color: Theme.accent
                    anchors {
                        left: parent.left
                        right: parent.right
                    }
                    y: 0
                    visible: treeDelegateId.isRule
                             && structureTreeId.dragLayer === treeDelegateId.layerIndex
                             && structureTreeId.dragCategory === treeDelegateId.category
                             && structureTreeId.dragTo === treeDelegateId.ruleIndex
                }

                // Insert-below: only on the category's last rule, so an
                // end-of-block drop has somewhere to show (the next row is another
                // category group or layer).
                QQ.Rectangle {
                    height: 2
                    color: Theme.accent
                    anchors {
                        left: parent.left
                        right: parent.right
                    }
                    y: treeDelegateId.height - height
                    visible: treeDelegateId.isRule
                             && structureTreeId.dragLayer === treeDelegateId.layerIndex
                             && structureTreeId.dragCategory === treeDelegateId.category
                             && treeDelegateId.categoryLast
                             && structureTreeId.dragTo === treeDelegateId.ruleIndex + 1
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.flowSpacing

            QQ.Item { Layout.fillWidth: true }

            QC.Button {
                objectName: "brushEditorDiscardButton"
                text: "Discard"
                enabled: editorId.dirty
                onClicked: editorId.discard()
            }

            QC.Button {
                objectName: "brushEditorApplyButton"
                text: editorId.paletteWritable ? "Apply" : "Duplicate & Save…"
                enabled: editorId.dirty
                highlighted: true
                onClicked: editorId.apply()
            }
        }
    }
}
