/**************************************************************************
**
**    Copyright (C) 2026
**
**************************************************************************/

import QtQuick.Controls as QC
import cavewherelib

QC.MenuItem {

    required property SurveyEditorModel model
    required property cwSurveyEditorCellIndex cell
    property RemovePreview removePreview: null

    text: "Split chunk here"
    enabled: model !== null && model.canSplitChunkAt(cell)

    onHighlightedChanged: {
        if(removePreview === null || model === null) {
            return
        }
        if(highlighted) {
            removePreview.previewSplit(model.chunkForRow(cell.modelRow),
                                       model.data(model.index(cell.modelRow, 0), SurveyEditorModel.IndexInChunkRole))
        } else {
            removePreview.clear()
        }
    }

    onTriggered: {
        if(removePreview !== null) {
            removePreview.clear()
        }
        model.splitChunkAt(cell)
    }
}
