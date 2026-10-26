/**************************************************************************
**
**    Copyright (C) 2026
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QC.MenuItem {
    id: mergeItemId

    required property SurveyEditorModel model
    required property SurveyChunk chunk
    required property int direction
    property RemovePreview removePreview: null
    property bool tieMatches: canMerge()

    function canMerge() : bool {
        if(model === null || chunk === null) {
            return false
        }
        return direction === SurveyChunk.Above ? model.canMergeWithAbove(chunk)
                                               : model.canMergeWithBelow(chunk)
    }

    // The chunk that goes away: the lower one of the pair
    function vanishingChunk() : SurveyChunk {
        if(chunk === null || chunk.parentTrip === null) {
            return null
        }
        if(direction === SurveyChunk.Above) {
            return chunk
        }
        const trip = chunk.parentTrip
        for(let i = 0; i < trip.chunkCount - 1; i++) {
            if(trip.chunk(i) === chunk) {
                return trip.chunk(i + 1)
            }
        }
        return null
    }

    text: direction === SurveyChunk.Above ? "Merge with chunk above" : "Merge with chunk below"
    enabled: tieMatches

    onHighlightedChanged: {
        if(removePreview === null) {
            return
        }
        const vanishing = vanishingChunk()
        if(highlighted && vanishing !== null) {
            removePreview.previewChunk(vanishing)
        } else {
            removePreview.clear()
        }
    }

    onTriggered: {
        if(removePreview !== null) {
            removePreview.clear()
        }
        if(direction === SurveyChunk.Above) {
            model.mergeWithAbove(chunk)
        } else {
            model.mergeWithBelow(chunk)
        }
    }

    //The menu stays built between popups, so station names can change under it
    QQ.Connections {
        target: mergeItemId.menu

        function onAboutToShow() {
            mergeItemId.tieMatches = mergeItemId.canMerge()
        }
    }
}
