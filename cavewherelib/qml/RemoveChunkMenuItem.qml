/**************************************************************************
**
**    Copyright (C) 2026
**
**************************************************************************/

import QtQuick.Controls as QC
import cavewherelib

QC.MenuItem {
    required property SurveyChunk chunk
    property RemovePreview removePreview: null

    text: "Remove Chunk"
    enabled: chunk !== null && chunk.parentTrip !== null

    onHighlightedChanged: {
        if(removePreview === null) {
            return
        }
        if(highlighted) {
            removePreview.previewChunk(chunk)
        } else {
            removePreview.clear()
        }
    }

    onTriggered: {
        if(chunk !== null && chunk.parentTrip !== null) {
            if(removePreview !== null) {
                removePreview.clear()
            }
            chunk.parentTrip.removeChunk(chunk)
        }
    }
}
