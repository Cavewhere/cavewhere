import QtQuick
import cavewherelib

QtObject {
    required property Trip trip
    required property ListView view;
    required property SurveyEditorModel model;

    property cwSurveyEditorRowIndex rowIndex;
    property int chunkDataRole;

    // property SurveyChunk chunk
    // property int indexInChunk: -1
    // property int chunkRole: -1
    // property int rowType: -1

    function focusOnLastChunk() {
        // console.log("Focus on last!")
        let lastChunkIndex = trip.chunkCount - 1
        let lastChunk = trip.chunk(lastChunkIndex);
        if(lastChunk.stationCount() > 0) {
            let lastRowIndex = model.rowIndex(lastChunk, 0, SurveyEditorModel.StationRow)
            let stationRow = model.toModelRow(lastRowIndex);
            view.currentIndex = stationRow;
            view.forceActiveFocus()

            chunkDataRole = SurveyChunk.StationNameRole;
            rowIndex = lastRowIndex;
        }
    }
}
