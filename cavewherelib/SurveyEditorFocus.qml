import QtQuick
import cavewherelib

QtObject {
    required property Trip trip
    required property ListView view;
    required property SurveyEditorModel model;

    property cwSurveyEditorBoxIndex boxIndex;

    // property SurveyChunk chunk
    // property int indexInChunk: -1
    // property int chunkRole: -1
    // property int rowType: -1

    function focusOnLastChunk() {
        console.log("Focus on last!")
        let lastChunkIndex = trip.chunkCount - 1
        let lastChunk = trip.chunk(lastChunkIndex);
        if(lastChunk.stationCount() > 0) {
            let stationRow = model.toRow(SurveyEditorModel.StationRow, lastChunk, 0);
            view.currentIndex = stationRow;
            view.forceActiveFocus()

            boxIndex = model.boxIndex(SurveyEditorModel.StationRow, lastChunk, 0, SurveyChunk.StationNameRole)
        }
    }
}
