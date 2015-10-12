import QtQuick 2.0
import Cavewhere 1.0
import QtQuick.Layouts 1.1

Item {
    id: overviewId

    property Trip trip: null

    property var _chunkToWatch: [] //Array of chunks to watch
    property int _numWarnings: 0
    property int _numErrors: 0

    implicitWidth: borderRectangle.width
    implicitHeight: borderRectangle.height

//    visible: false

    function updateWarningAndErrors() {
        var numWarnings = 0;
        var numErrors = 0;

        if(trip != null) {

            for(var i = 0; i < trip.numberOfChunks; i++) {
                var chunk = trip.chunk(i);
                var errors = chunk.errors;

                for(var errorIndex in errors) {
                    var error = errors[errorIndex]
                    switch(error.type) {
                    case SurveyChunkError.Fatal:
                        numErrors++;
                        break;
                    case SurveyChunkError.Warning:
                        if(!error.suppressed) {
                            numWarnings++;
                        }
                        break;
                    }
                }
            }
        }

        _numWarnings = numWarnings;
        _numErrors = numErrors;
    }

    onTripChanged: updateWarningAndErrors()

    Rectangle {
        id: borderRectangle

        border.width: 1
//        border.color: "#930100"
//        color: "#FAB8B9"
        radius: 3
        width: layoutId.width + 20
        height: layoutId.height + 10

        RowLayout {
            id: layoutId
            anchors.centerIn: parent

            Image {
                id: stopImage
                source: "qrc:icons/stopSignError.png"
                visible: _numErrors > 0
            }

            Image {
                id: warningImage
                source: "qrc:icons/warning.png"
                visible: _numWarnings > 0
            }

            Text {
                id: errorText
            }
        }


    }

    Connections {
        target: trip
        onNumberOfChunksChanged: {
            //Disconnect old chunks
            for(var i in _chunkToWatch) {
                var chunk = _chunkToWatch[i]
                chunk.errorsChanged.disconnect(updateWarningAndErrors)
            }

            //Clear the chunks to watch
            _chunkToWatch = [];

            //Go through all the chunks
            for(var i = 0; i < trip.numberOfChunks; i++) {
                var chunk =  trip.chunk(i);
                chunk.errorsChanged.connect(updateWarningAndErrors)
                _chunkToWatch.push(chunk)
            }

            updateWarningAndErrors()
        }
    }

    onStateChanged: {
        console.log("State:" + state)
    }

    states: [
        State {
            name: "noErrorsOrWarnings"
            when: _numErrors == 0 && _numWarnings == 0

            PropertyChanges {
                target: overviewId
                visible: false
            }
        },

        State {
            name: "hasErrorOrWarning"
            PropertyChanges {
                target: overviewId
                visible: true
            }
        },

        State {
            name: "hasError"
            extend: "hasErrorOrWarning"

            PropertyChanges {
                target: borderRectangle
                border.color: "#930100"
                color: "#FAB8B9"
            }
        },

        State {
            name: "hasWarning"
            extend: "hasErrorOrWarning"


            PropertyChanges {
                target: borderRectangle
                border.color: "#E06841"
                color: "#FFFDBC"
            }
        },

        State {
            name: "onlyErrors"
            when: _numErrors > 0 && _numWarnings == 0
            extend: "hasError"
            PropertyChanges {
                target: errorText
                text: "There are <b>" + _numErrors + " errors</b>"
            }
        },

        State {
            name: "onlyWarnings"
            when: _numErrors == 0 && _numWarnings > 0
            extend: "hasWarning"

            PropertyChanges {
                target: errorText
                text: "There are " + _numWarnings + " warnings"
            }
        },

        State {
            name: "ErrorsAndWarnings"
            when: _numErrors > 0 && _numWarnings > 0
            extend: "hasErrors"

            PropertyChanges {
                target: errorText
                text: "There are <b>" + _numErrors + " errors</b> and " + _numWarnings + " warnings"
            }
        }
    ]
}

