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

    // Name of the brush new strokes are drawn with (resolved via the active
    // palette). A temporary fixed set until the brush picker lands in commit 7.
    property string brushName: "wall"

    // Eraser is a tool mode, not a brush (Decision 2). When true, pen input
    // erases instead of drawing. Pending the eraser redesign.
    property bool eraseActive: false

    // World-space eraser radius in meters. The canvas uses worldToScreenId to
    // convert this to the on-screen cursor size, so the visible disk matches
    // the actual erase region at any zoom.
    property real eraserRadius: 0.5

    padding: Theme.tightSpacing

    ColumnLayout {
        spacing: Theme.tightSpacing

        QC.ButtonGroup {
            id: kindGroupId
        }

        QC.RadioButton {
            id: wallButtonId
            objectName: "wallButton"
            text: "Wall"
            checked: !toolbarId.eraseActive && toolbarId.brushName === "wall"
            QC.ButtonGroup.group: kindGroupId
            onToggled: {
                if (checked) {
                    toolbarId.brushName = "wall"
                    toolbarId.eraseActive = false
                }
            }
        }

        QC.RadioButton {
            id: featureButtonId
            objectName: "featureButton"
            text: "Feature"
            checked: !toolbarId.eraseActive && toolbarId.brushName === "feature"
            QC.ButtonGroup.group: kindGroupId
            onToggled: {
                if (checked) {
                    toolbarId.brushName = "feature"
                    toolbarId.eraseActive = false
                }
            }
        }

        QC.RadioButton {
            id: scrapOutlineButtonId
            objectName: "scrapOutlineButton"
            text: "Scrap Outline"
            checked: !toolbarId.eraseActive && toolbarId.brushName === "scrap-outline"
            QC.ButtonGroup.group: kindGroupId
            onToggled: {
                if (checked) {
                    toolbarId.brushName = "scrap-outline"
                    toolbarId.eraseActive = false
                }
            }
        }

        QC.RadioButton {
            id: eraserButtonId
            objectName: "eraserButton"
            text: "Eraser"
            checked: toolbarId.eraseActive
            QC.ButtonGroup.group: kindGroupId
            onToggled: {
                if (checked) {
                    toolbarId.eraseActive = true
                }
            }
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
