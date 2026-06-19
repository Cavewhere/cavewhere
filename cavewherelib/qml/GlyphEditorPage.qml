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

// Authors a single glyph against a writable palette, injected directly (no trip
// or region). Reached via RootData.pageSelectionModel with `palette` and an
// optional `glyphName` set as page properties (SketchToolbar's "Edit glyphs…").
// The drawing surface reuses SketchInputArea — the same pan/zoom/pen layer as
// the survey-sketch editor — over the canvas's internal cwSketch.
StandardPage {
    id: page
    objectName: "glyphEditorPage"

    // Set via pageProperties at registration.
    property SymbologyPalette palette
    property string glyphName: ""
    property string glyphDisplayName: ""

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

    QQ.Component.onCompleted: {
        nameFieldId.text = page.glyphName
        displayFieldId.text = page.glyphDisplayName
        glyphCanvasId.loadGlyphNamed(page.glyphName)
    }

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

    // Chrome: identity fields, brush picker, and the edit/save controls.
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
                    onClicked: glyphCanvasId.sketch.undoStack.undo()
                }

                QC.Button {
                    objectName: "glyphRedoButton"
                    text: "Redo"
                    enabled: glyphCanvasId.sketch.undoStack.canRedo
                    onClicked: glyphCanvasId.sketch.undoStack.redo()
                }

                QC.Button {
                    objectName: "glyphClearButton"
                    text: "Clear"
                    onClicked: glyphCanvasId.clear()
                }
            }

            RowLayout {
                spacing: Theme.tightSpacing

                QC.Button {
                    objectName: "glyphSaveButton"
                    text: "Save"
                    enabled: nameFieldId.text.length > 0
                    onClicked: {
                        const glyph = glyphCanvasId.toGlyph(nameFieldId.text, displayFieldId.text)
                        if (RootData.paletteManager.saveGlyph(page.palette, glyph)) {
                            RootData.pageSelectionModel.back()
                        }
                    }
                }

                QC.Button {
                    objectName: "glyphCancelButton"
                    text: "Cancel"
                    onClicked: RootData.pageSelectionModel.back()
                }
            }
        }
    }
}
