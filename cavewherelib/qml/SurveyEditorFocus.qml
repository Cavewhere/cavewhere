import QtQuick
import cavewherelib

QtObject {
    required property Trip trip
    required property ListView view;
    required property SurveyEditorModel model;

    property int row: -1
    property int role: -1

    function setIndex(newRow, newRole) {
        if(model.isCellValid(model.cellIndex(newRow, newRole))) {
            role = newRole
            row = newRow
        }
    }

    function focusOnLastChunk() {
        let lastChunkIndex = trip.chunkCount - 1
        let lastChunk = trip.chunk(lastChunkIndex);
        if(lastChunk.stationCount > 0) {
            let lastRow = model.modelRowForChunkRole(lastChunk, 0, SurveyChunk.StationNameRole)
            setIndex(lastRow, SurveyChunk.StationNameRole)
        }
    }
}
