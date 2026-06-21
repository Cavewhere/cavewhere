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

        QQ.TreeView {
            id: structureTreeId
            objectName: "brushStructureTree"

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: editorId.structureModel

            // Layers and their rules are always shown expanded (mirrors the
            // phase-1 always-open tree); re-expand whenever the model resets on a
            // brush load or discard.
            QQ.Component.onCompleted: expandRecursively()

            QQ.Connections {
                target: editorId.structureModel
                // The view rebuilds its rows asynchronously after a reset, so
                // defer the expand until those rows exist.
                function onModelReset() { Qt.callLater(structureTreeId.expandRecursively) }
            }

            // A plain Item delegate (not TreeViewDelegate) driven by TreeView's
            // attached properties: the native style forbids customizing a
            // TreeViewDelegate's contentItem, and a rule row needs a checkbox.
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
                required property bool ruleEnabled
                required property string label

                readonly property real kIndent: Theme.sectionSpacing

                implicitWidth: structureTreeId.width
                implicitHeight: contentRowId.implicitHeight + 2 * Theme.tightSpacing

                RowLayout {
                    id: contentRowId

                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        // Indent only the tree column, per the depth in the model.
                        leftMargin: treeDelegateId.isTreeNode ? treeDelegateId.depth * treeDelegateId.kIndent : 0
                    }
                    spacing: Theme.tightSpacing

                    // Expand/collapse indicator on tree-column rows that have
                    // children; a same-width spacer keeps leaf rows aligned.
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

                    QC.Label {
                        visible: treeDelegateId.isLayer
                        text: treeDelegateId.label === "" ? "Line layer" : treeDelegateId.label
                        color: Theme.textSubtle
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        Layout.fillWidth: true
                    }

                    QC.CheckBox {
                        visible: !treeDelegateId.isLayer
                        objectName: "ruleCheckBox_" + treeDelegateId.layerIndex + "_" + treeDelegateId.ruleIndex
                        text: treeDelegateId.label
                        checked: treeDelegateId.ruleEnabled
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
