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

QC.Frame {
    id: toolbarId

    property Sketch sketch

    // The canvas owning currentBrushName — the single source of truth the brush
    // picker writes (3b). The toolbar drives it but does not store the brush
    // itself.
    property SketchCanvas canvas

    // Eraser is a tool mode, not a brush (Decision 2). When true, pen input
    // erases instead of drawing. Pending the eraser redesign.
    property bool eraseActive: false

    // World-space eraser radius in meters. The canvas uses worldToScreenId to
    // convert this to the on-screen cursor size, so the visible disk matches
    // the actual erase region at any zoom.
    property real eraserRadius: 0.5

    // The glyph-editor page we last opened, tracked so re-opening unregisters
    // the stale one rather than leaking pages.
    property QQ.QtObject _glyphEditorPage: null

    // GlyphEditorPage loaded as a standalone component (from its own URL) rather
    // than an inline Component: this file is `ComponentBehavior: Bound`, so an
    // inline component would be tied to this toolbar's creation context and
    // cwPageView could not instantiate it ("Cannot instantiate bound component
    // outside its creation context"). The URL-loaded component is free of that
    // binding and instantiable in the page view's context.
    property QQ.Component _glyphEditorComponent: null

    padding: Theme.tightSpacing

    function _selectBrush(name: string) {
        if (toolbarId.canvas) {
            toolbarId.canvas.currentBrushName = name
        }
        toolbarId.eraseActive = false
    }

    // Open the glyph editor on `targetPalette` (must be writable). Registers a
    // fresh page carrying the palette as a property and navigates to it.
    function _openGlyphEditor(targetPalette: SymbologyPalette) {
        if (!targetPalette) {
            return
        }
        if (toolbarId._glyphEditorComponent === null) {
            toolbarId._glyphEditorComponent =
                Qt.createComponent(Qt.resolvedUrl("GlyphEditorPage.qml"))
        }
        if (toolbarId._glyphEditorPage) {
            RootData.pageSelectionModel.unregisterPage(toolbarId._glyphEditorPage)
        }
        toolbarId._glyphEditorPage = RootData.pageSelectionModel.registerPage(
            null, "Glyph Editor", toolbarId._glyphEditorComponent, { palette: targetPalette })
        RootData.pageSelectionModel.gotoPage(toolbarId._glyphEditorPage)
    }

    ColumnLayout {
        spacing: Theme.tightSpacing

        QC.Label {
            text: "Palette"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            color: Theme.textSubtle
        }

        QC.ComboBox {
            id: paletteComboId
            objectName: "paletteCombo"
            Layout.fillWidth: true
            textRole: "name"
            valueRole: "id"

            model: SymbologyPaletteListModel {
                manager: RootData.paletteManager
            }

            // Reflect the resolved palette (may be inherited from settings/default
            // when the region has no id of its own), not just the region's id.
            // Reading `count` makes the binding re-run once the model populates —
            // indexOfValue() alone isn't reactive to model row changes.
            currentIndex: {
                const ignored = paletteComboId.count
                const resolvedId = (toolbarId.sketch && toolbarId.sketch.resolvedPalette)
                    ? toolbarId.sketch.resolvedPalette.id
                    : undefined
                return paletteComboId.indexOfValue(resolvedId)
            }

            // onActivated fires only on user selection, so it never loops with
            // the currentIndex binding above.
            onActivated: RootData.region.defaultPaletteId = currentValue
        }

        // Fork a read-only palette so its glyphs can be edited.
        QC.Button {
            objectName: "duplicatePaletteButton"
            Layout.fillWidth: true
            text: "Duplicate to edit…"
            visible: toolbarId.sketch !== null
                     && toolbarId.sketch.resolvedPalette !== null
                     && !toolbarId.sketch.resolvedPalette.writable
            onClicked: {
                const fork = RootData.paletteManager.duplicatePalette(
                    toolbarId.sketch.resolvedPalette,
                    toolbarId.sketch.resolvedPalette.name + " (copy)")
                toolbarId._openGlyphEditor(fork)
            }
        }

        // Edit the glyphs of a writable palette.
        QC.Button {
            objectName: "editGlyphsButton"
            Layout.fillWidth: true
            text: "Edit glyphs…"
            visible: toolbarId.sketch !== null
                     && toolbarId.sketch.resolvedPalette !== null
                     && toolbarId.sketch.resolvedPalette.writable
            onClicked: toolbarId._openGlyphEditor(toolbarId.sketch.resolvedPalette)
        }

        QC.Label {
            text: "Brush"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            color: Theme.textSubtle
        }

        BrushPicker {
            id: brushPickerId
            objectName: "brushPicker"
            Layout.fillWidth: true
            palette: toolbarId.sketch ? toolbarId.sketch.resolvedPalette : null
            brushName: toolbarId.canvas ? toolbarId.canvas.currentBrushName : ""
            onBrushSelected: (name) => toolbarId._selectBrush(name)
        }

        QC.CheckBox {
            id: eraserButtonId
            objectName: "eraserButton"
            text: "Eraser"
            checked: toolbarId.eraseActive
            onToggled: toolbarId.eraseActive = checked
        }

        RowLayout {
            visible: toolbarId.eraseActive
            spacing: Theme.tightSpacing

            QC.Label { text: "Size" }

            QC.Slider {
                id: eraserSizeSliderId
                objectName: "eraserSizeSlider"
                from: 0.1
                to: 2.0
                stepSize: 0.1
                value: toolbarId.eraserRadius
                Layout.preferredWidth: 100
                onMoved: toolbarId.eraserRadius = value
            }
        }

        QC.Button {
            id: undoButtonId
            objectName: "undoButton"
            text: "Undo"
            enabled: toolbarId.sketch !== null && toolbarId.sketch.undoStack.canUndo
            onClicked: toolbarId.sketch.undoStack.undo()
        }

        QC.Button {
            id: redoButtonId
            objectName: "redoButton"
            text: "Redo"
            enabled: toolbarId.sketch !== null && toolbarId.sketch.undoStack.canRedo
            onClicked: toolbarId.sketch.undoStack.redo()
        }

        QC.Button {
            id: clearButtonId
            objectName: "clearButton"
            text: "Clear"
            enabled: toolbarId.sketch !== null && toolbarId.sketch.undoStack.canUndo
            onClicked: toolbarId.sketch.clearStrokes()
        }
    }
}
