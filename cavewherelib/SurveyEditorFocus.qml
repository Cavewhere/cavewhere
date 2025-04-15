import QtQuick
import cavewherelib

QtObject {
    required property Trip trip
    required property ListView view;
    required property SurveyEditorModel model;

    property cwSurveyEditorBoxIndex boxIndex;

    function setIndex(newBoxIndex) {
        if(newBoxIndex.chunk && newBoxIndex.indexInChunk >= 0)
        {
            boxIndex = newBoxIndex
        }
    }

    function focusOnLastChunk() {
        let lastChunkIndex = trip.chunkCount - 1
        let lastChunk = trip.chunk(lastChunkIndex);
        if(lastChunk.stationCount > 0) {
            let lastBoxIndex = model.boxIndex(lastChunk, 0, SurveyEditorRowIndex.StationRow, SurveyChunk.StationNameRole);
            view.currentIndex = model.toModelRow(lastBoxIndex.rowIndex);

            setIndex(lastBoxIndex);
            boxIndex = lastBoxIndex
        }
    }
}
