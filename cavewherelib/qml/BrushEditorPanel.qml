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

    // STOPGAP (phase 1): the structure tree is built from invokable accessors
    // (layerCount/ruleCount/...) which QML can't track as binding dependencies,
    // so this counter is bumped on structureChanged and smuggled into each
    // binding via the comma operator to force re-evaluation. Phase 2 replaces the
    // whole nested-Repeater + revision-counter scheme with a TreeView backed by a
    // real QAbstractItemModel on cwBrushEditor. See SYMBOLOGY_PALETTE_COMMIT9_PLAN.
    property int _structureRevision: 0

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

    QQ.Connections {
        target: editorId

        function onStructureChanged() { panel._structureRevision++ }
        function onBrushLoaded() { panel._structureRevision++ }
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

        QC.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: panel.width - 2 * Theme.pageMargin
                spacing: Theme.flowSpacing

                QQ.Repeater {
                    model: (panel._structureRevision, editorId.loaded ? editorId.layerCount() : 0)

                    delegate: ColumnLayout {
                        id: layerDelegateId

                        required property int index

                        readonly property string label: (panel._structureRevision, editorId.layerLabel(index))

                        Layout.fillWidth: true
                        spacing: Theme.tightSpacing

                        QC.Label {
                            text: layerDelegateId.label === "" ? "Line layer" : layerDelegateId.label
                            color: Theme.textSubtle
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCaption
                            topPadding: Theme.tightSpacing
                        }

                        QQ.Repeater {
                            model: (panel._structureRevision, editorId.ruleCount(layerDelegateId.index))

                            delegate: QC.CheckBox {
                                id: ruleCheckId

                                required property int index

                                readonly property int layerIndex: layerDelegateId.index

                                objectName: "ruleCheckBox_" + layerIndex + "_" + index
                                Layout.fillWidth: true
                                Layout.leftMargin: Theme.sectionSpacing
                                text: (panel._structureRevision, editorId.ruleName(layerIndex, index))
                                checked: (panel._structureRevision, editorId.ruleEnabled(layerIndex, index))
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeBody

                                onToggled: editorId.setRuleEnabled(layerIndex, index, checked)
                            }
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
