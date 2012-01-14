import QtQuick 1.0
import Cavewhere 1.0

DataBox {
    id: stationBox

    onFocused: {
        var lastStationIndex = surveyChunk.stationCount() - 1
        console.debug("Focus station " + lastStationIndex + " " + rowIndex);

        //Try to guess for new stations what the next station is
        //Make sure the station is the last station in the chunk
        if(lastStationIndex  === rowIndex) {

            console.debug("DataValue" + dataValue)

            //Make sure the data is empty
            if(dataValue == "") {
                var stationName = surveyChunk.guessLastStationName();
                surveyChunk.setData(dataRole, rowIndex, stationName);
            }
        }
    }
}
