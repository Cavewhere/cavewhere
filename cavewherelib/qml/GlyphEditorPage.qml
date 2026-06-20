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

// A glyph-library manager for one writable palette, injected directly (no trip
// or region). Reached via RootData.pageSelectionModel with `palette` set as a
// page property (SketchToolbar's "Edit glyphs…"). Master-detail: the glyph list
// on the left, the drawing editor on the right. Edits auto-save (no Save/Cancel)
// — object-first through saveGlyph, persisted asynchronously by cwSaveLoad.
//
// Wide-only for now: a SplitView side-by-side. Compact (isNarrow) behavior is a
// follow-up; below the panel-collapse breakpoint the split is cramped.
StandardPage {
    id: page
    objectName: "glyphEditorPage"

    // Set via pageProperties at registration.
    property SymbologyPalette palette

    // The glyph currently loaded in the editor; "" is an unnamed draft that is
    // not yet a palette member.
    property string currentName: ""

    // A draft carries strokes but no valid name yet, so it can't be saved — the
    // name field is flagged until the cartographer names it.
    property bool needsName: false

    readonly property bool _writable: page.palette !== null && page.palette.writable

    // world (glyph-local) -> item pixels. The map matrix is a pure scale + Y-flip
    // (world +Y up, item +Y down), so this is scalar math, matching
    // SketchInputArea._worldPoint inverted.
    function _itemPoint(worldX: real, worldY: real): point {
        const ppm = inputAreaId.pxPerMeter
        const pan = inputAreaId.pan
        return Qt.point(worldX * ppm + pan.x, -worldY * ppm + pan.y)
    }

    // paper-mm -> world-m at the canvas scale (1:250 default): the same
    // 1 / (1000 * scaleRatio) factor cwGlyphTessellationCache::paperMmToWorldM uses.
    readonly property real _paperFactor: {
        const ratio = glyphCanvasId.sketch ? glyphCanvasId.sketch.mapScale.scale : 0.0
        return ratio > 0.0 ? 1.0 / (1000.0 * ratio) : 0.25
    }
    readonly property real _paperHalfWorldX: glyphCanvasId.paperExtentMm.width * _paperFactor / 2.0
    readonly property real _paperHalfWorldY: glyphCanvasId.paperExtentMm.height * _paperFactor / 2.0

    // Auto-save the editor's current glyph onto the palette (object-first;
    // cwSaveLoad persists the .cwglyph file asynchronously). A draft is
    // materialized only once it carries a valid kebab-case name, so an unnamed
    // scribble never lands in the palette; until then the name field is flagged.
    function _autoSave() {
        if (!page._writable) {
            return
        }
        // An empty sheet is never persisted: a fresh draft, or a Clear/undo back
        // to zero strokes, must not overwrite a saved glyph with a blank one.
        // Removing a glyph is the Remove flow's job, not a side effect of Clear.
        if (glyphCanvasId.sketch.strokeCount() === 0) {
            page.needsName = false
            return
        }
        if (!SymbologyName.isValidName(nameFieldId.text)) {
            page.needsName = true
            return
        }
        page.needsName = false
        const oldName = page.currentName   // "" for a fresh Add / unnamed draft
        const newName = nameFieldId.text
        // Set currentName before saving: saveGlyph fires glyphChanged synchronously,
        // which resets the model and re-syncs the list selection off currentName.
        page.currentName = newName
        const glyph = glyphCanvasId.toGlyph(newName, displayFieldId.text)
        RootData.paletteManager.saveGlyph(page.palette, glyph)

        // Rename = save-new + drop-old (name is the identity key). Save first so
        // the new row exists, then remove the orphan; currentName already equals
        // newName, so both resets re-anchor selection on the surviving row.
        if (oldName !== "" && oldName !== newName) {
            RootData.paletteManager.removeGlyph(page.palette, oldName)
        }
    }

    function _selectGlyph(name: string, displayName: string) {
        page.needsName = false
        page.currentName = name
        glyphCanvasId.loadGlyphNamed(name)
        nameFieldId.text = name
        displayFieldId.text = displayName
    }

    // Select a glyph known only by name (e.g. a fresh duplicate): look its
    // display name up on the palette, load it, then anchor the list highlight.
    // The model is already reset by the time we're called, so indexOfName hits.
    function _selectGlyphByName(name: string) {
        const displayName = page.palette ? page.palette.glyphDisplayName(name) : ""
        page._selectGlyph(name, displayName)
        glyphListId.currentIndex = glyphModelId.indexOfName(name)
    }

    // Start a fresh unnamed draft: a blank sheet that only becomes a palette
    // member once named and auto-saved.
    function _addGlyph() {
        page.needsName = false
        page.currentName = ""
        glyphCanvasId.clear()
        nameFieldId.text = ""
        displayFieldId.text = ""
        glyphListId.currentIndex = -1
        nameFieldId.forceActiveFocus()
    }

    // Open on a blank draft; the cartographer picks an existing glyph from the
    // list or starts drawing a new one. (Auto-selecting the first row would lean
    // on ListView.currentItem before its delegate is realized.)
    QQ.Component.onCompleted: nameFieldId.forceActiveFocus()

    // Drawing a stroke auto-commits the current glyph.
    QQ.Connections {
        target: glyphCanvasId.sketch
        function onStrokeEnded() { page._autoSave() }
    }

    // The model re-sorts on every glyphChanged (full reset), so the ListView's
    // index-based currentIndex would drift onto whatever row slid into place.
    // Re-anchor the highlight to the glyph the editor is on (and select a
    // freshly-named draft once it materializes).
    QQ.Connections {
        target: glyphModelId
        function onModelReset() {
            glyphListId.currentIndex = glyphModelId.indexOfName(page.currentName)
        }
    }

    // Right-click → Remove confirmation. The ListView has no proxy, so a
    // delegate row maps straight to the glyph name carried by removeName.
    RemoveAskBox {
        id: removeChallengeId
        onRemove: {
            RootData.paletteManager.removeGlyph(page.palette, removeName)
            // If the editor was on the removed glyph, drop back to a blank draft;
            // removing some other row leaves the current edit untouched.
            if (removeName === page.currentName) {
                page._addGlyph()
            }
        }
    }

    QC.SplitView {
        anchors.fill: parent
        orientation: QQ.Qt.Horizontal

        // Left rail: the glyph list with an Add header.
        ColumnLayout {
            objectName: "glyphLibraryRail"
            QC.SplitView.preferredWidth: Theme.glyphRailWidth
            QC.SplitView.minimumWidth: Theme.glyphRailMinWidth
            spacing: 0

            QC.Frame {
                Layout.fillWidth: true
                padding: Theme.tightSpacing

                RowLayout {
                    anchors.fill: parent
                    spacing: Theme.tightSpacing

                    QC.Label {
                        Layout.fillWidth: true
                        text: "Glyphs"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textSubtle
                    }

                    QC.Button {
                        objectName: "addGlyphButton"
                        text: "+ Add"
                        enabled: page._writable
                        onClicked: page._addGlyph()
                    }
                }
            }

            // ListView is itself a Flickable, so it scrolls on its own — no
            // ScrollView wrapper (which would leave it unsized and unrealized).
            QQ.ListView {
                id: glyphListId
                objectName: "glyphList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                enabled: page._writable

                model: GlyphListModel {
                    id: glyphModelId
                    palette: page.palette
                }

                delegate: GlyphListDelegate {
                    id: glyphDelegateId
                    width: glyphListId.width
                    highlighted: QQ.ListView.isCurrentItem
                    onClicked: {
                        glyphListId.currentIndex = glyphDelegateId.index
                        page._selectGlyph(glyphDelegateId.name, glyphDelegateId.displayName)
                    }

                    GlyphContextMenu {
                        anchors.fill: parent
                        removeChallenge: removeChallengeId
                        row: glyphDelegateId.index
                        name: glyphDelegateId.name
                        writable: page._writable
                        onDuplicateRequested: (sourceName) => {
                            const newName = RootData.paletteManager.duplicateGlyph(page.palette, sourceName)
                            if (newName.length > 0) {
                                page._selectGlyphByName(newName)
                            }
                        }
                    }
                }

                QC.ScrollBar.vertical: QC.ScrollBar {}
            }
        }

        // Right pane: the drawing editor.
        QQ.Item {
            objectName: "glyphEditorPane"
            QC.SplitView.fillWidth: true

            SketchGlyphCanvas {
                id: glyphCanvasId
                objectName: "glyphCanvas"
                anchors.fill: parent
                palette: page.palette
                zoom: inputAreaId.zoom
                pan: inputAreaId.pan
                mapMatrix: inputAreaId.worldToScreen.matrix
                activeStrokeIndex: inputAreaId.activeStrokeIndex
                grid: inputAreaId.grid

                pathModel.activeStrokeColor: Theme.sketchStrokeWall
            }

            // Paper-extent rectangle, drawn over the canvas.
            QQ.Rectangle {
                objectName: "paperExtentOverlay"
                readonly property point _topLeft: page._itemPoint(-page._paperHalfWorldX, page._paperHalfWorldY)

                x: _topLeft.x
                y: _topLeft.y
                width: page._paperHalfWorldX * 2.0 * inputAreaId.pxPerMeter
                height: page._paperHalfWorldY * 2.0 * inputAreaId.pxPerMeter
                color: Theme.transparent
                border.color: Theme.sketchGridLine
                border.width: 1
            }

            // Origin crosshair at world (0,0).
            QQ.Item {
                objectName: "originCrosshair"
                readonly property point _origin: page._itemPoint(0, 0)
                readonly property int kArm: 8

                x: _origin.x
                y: _origin.y

                QQ.Rectangle {
                    anchors.centerIn: parent
                    width: parent.kArm * 2
                    height: 1
                    color: Theme.sketchGridLabel
                }
                QQ.Rectangle {
                    anchors.centerIn: parent
                    width: 1
                    height: parent.kArm * 2
                    color: Theme.sketchGridLabel
                }
            }

            // +X tangent axis: the direction a glyph's +X aligns to the centerline.
            QQ.Item {
                objectName: "tangentAxis"
                readonly property point _origin: page._itemPoint(0, 0)
                readonly property point _tip: page._itemPoint(page._paperHalfWorldX, 0)

                QQ.Rectangle {
                    x: parent._origin.x
                    y: parent._origin.y
                    width: parent._tip.x - parent._origin.x
                    height: 2
                    color: Theme.sketchStrokeWall
                }

                QC.Label {
                    x: parent._tip.x + Theme.tightSpacing
                    y: parent._tip.y - height / 2
                    text: "+X (tangent)"
                    color: Theme.textSubtle
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                }
            }

            SketchInputArea {
                id: inputAreaId
                objectName: "glyphInputArea"
                anchors.fill: parent
                sketch: glyphCanvasId.sketch
                brushName: glyphCanvasId.currentBrushName
            }

            // Chrome: identity fields, brush picker, and the edit controls.
            QC.Frame {
                id: chromeId
                objectName: "glyphEditorChrome"
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: Theme.pageMargin
                padding: Theme.tightSpacing

                ColumnLayout {
                    spacing: Theme.tightSpacing

                    QC.Label {
                        text: "Glyph name"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textSubtle
                    }

                    QC.TextField {
                        id: nameFieldId
                        objectName: "glyphNameField"
                        Layout.fillWidth: true
                        placeholderText: "kebab-case-name"
                        onEditingFinished: page._autoSave()

                        ErrorHelpBox {
                            objectName: "glyphNameHelpBox"
                            text: "Name this glyph to save it"
                            anchors.bottom: undefined
                            anchors.horizontalCenter: undefined
                            y: parent.height + Theme.tightSpacing
                            visible: page.needsName
                        }
                    }

                    QC.Label {
                        text: "Display name"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textSubtle
                    }

                    QC.TextField {
                        id: displayFieldId
                        objectName: "glyphDisplayNameField"
                        Layout.fillWidth: true
                        onEditingFinished: page._autoSave()
                    }

                    QC.Label {
                        text: "Brush"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textSubtle
                    }

                    BrushPicker {
                        id: brushPickerId
                        objectName: "glyphBrushPicker"
                        Layout.fillWidth: true
                        palette: page.palette
                        brushName: glyphCanvasId.currentBrushName
                        onBrushSelected: (name) => glyphCanvasId.currentBrushName = name
                    }

                    RowLayout {
                        spacing: Theme.tightSpacing

                        QC.Button {
                            objectName: "glyphUndoButton"
                            text: "Undo"
                            enabled: glyphCanvasId.sketch.undoStack.canUndo
                            onClicked: {
                                glyphCanvasId.sketch.undoStack.undo()
                                page._autoSave()
                            }
                        }

                        QC.Button {
                            objectName: "glyphRedoButton"
                            text: "Redo"
                            enabled: glyphCanvasId.sketch.undoStack.canRedo
                            onClicked: {
                                glyphCanvasId.sketch.undoStack.redo()
                                page._autoSave()
                            }
                        }

                        QC.Button {
                            objectName: "glyphClearButton"
                            text: "Clear"
                            onClicked: {
                                glyphCanvasId.clear()
                                page._autoSave()
                            }
                        }
                    }

                    QC.Button {
                        objectName: "glyphDoneButton"
                        Layout.fillWidth: true
                        text: "Done"
                        onClicked: RootData.pageSelectionModel.back()
                    }
                }
            }
        }
    }
}
