/**************************************************************************
**
**    Copyright (C) 2026
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

QQ.QtObject {
    id: removePreview

    property SurveyChunk chunk: null
    property int stationIndex: -1
    property int shotIndex: -1
    property bool previewChunkRemoval: false

    //A split draws a divider rather than striking rows out
    property SurveyChunk splitChunk: null
    property int splitShotIndex: -1

    function clear() {
        chunk = null
        stationIndex = -1
        shotIndex = -1
        previewChunkRemoval = false
        splitChunk = null
        splitShotIndex = -1
    }

    function previewChunk(previewChunk) {
        clear()
        if(previewChunk === null) {
            return
        }
        chunk = previewChunk
        previewChunkRemoval = true
    }

    function previewStation(previewChunk, previewStationIndex, direction) {
        clear()
        if(previewChunk === null) {
            return
        }
        chunk = previewChunk
        stationIndex = previewStationIndex
        if(direction === SurveyChunk.Above) {
            if(previewStationIndex > 0) {
                shotIndex = previewStationIndex - 1
            }
        } else if(direction === SurveyChunk.Below) {
            if(previewStationIndex < previewChunk.shotCount) {
                shotIndex = previewStationIndex
            }
        }
    }

    function previewShot(previewChunk, previewShotIndex, direction) {
        clear()
        if(previewChunk === null) {
            return
        }
        chunk = previewChunk
        shotIndex = previewShotIndex
        if(direction === SurveyChunk.Above) {
            stationIndex = previewShotIndex
        } else if(direction === SurveyChunk.Below) {
            stationIndex = previewShotIndex + 1
        }
    }

    function previewSplit(previewChunk, previewShotIndex) {
        clear()
        if(previewChunk === null) {
            return
        }
        splitChunk = previewChunk
        splitShotIndex = previewShotIndex
    }
}
