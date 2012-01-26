import QtQuick 1.0
import Cavewhere 1.0

DataBox {
    id: stationBox

    onFocusChanged: {
        if(focus) {
            var lastStationIndex = surveyChunk.stationCount() - 1

            //Try to guess for new stations what the next station is
            //Make sure the station is the last station in the chunk
            if(lastStationIndex  === rowIndex) {

                //Make sure the data is empty
                if(dataValue == "") {
                    var stationName = surveyChunk.guessLastStationName();
                    surveyChunk.setData(dataRole, rowIndex, stationName);
                }
            }
        }
    }
}
