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
    property int strokeKind: PenStroke.Wall

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
            checked: toolbarId.strokeKind === PenStroke.Wall
            QC.ButtonGroup.group: kindGroupId
            onToggled: {
                if (checked) {
                    toolbarId.strokeKind = PenStroke.Wall
                }
            }
        }

        QC.RadioButton {
            id: featureButtonId
            objectName: "featureButton"
            text: "Feature"
            checked: toolbarId.strokeKind === PenStroke.Feature
            QC.ButtonGroup.group: kindGroupId
            onToggled: {
                if (checked) {
                    toolbarId.strokeKind = PenStroke.Feature
                }
            }
        }

        QC.RadioButton {
            id: scrapOutlineButtonId
            objectName: "scrapOutlineButton"
            text: "Scrap Outline"
            checked: toolbarId.strokeKind === PenStroke.ScrapOutline
            QC.ButtonGroup.group: kindGroupId
            onToggled: {
                if (checked) {
                    toolbarId.strokeKind = PenStroke.ScrapOutline
                }
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
