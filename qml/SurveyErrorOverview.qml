import QtQuick 2.0 as QQ
import Cavewhere 1.0
import QtQuick.Layouts 1.1

QQ.Item {
    id: overviewId

    property Trip trip: null

    property var _chunkToWatch: [] //Array of chunks to watch
    property int _numWarnings: trip.errorModel.warningCount
    property int _numErrors: trip.errorModel.fatalCount

    implicitWidth: borderRectangle.width
    implicitHeight: borderRectangle.height

    visible: false

    QQ.Rectangle {
        id: borderRectangle

        border.width: 1
        radius: 3
        width: layoutId.width + 20
        height: layoutId.height + 10

        RowLayout {
            id: layoutId
            anchors.centerIn: parent

            QQ.Image {
                id: stopImage
                source: "qrc:icons/stopSignError.png"
                visible: _numErrors > 0
            }

            QQ.Image {
                id: warningImage
                source: "qrc:icons/warning.png"
                visible: _numWarnings > 0
            }

            Text {
                id: errorText
            }
        }


    }

    states: [
        QQ.State {
            name: "noErrorsOrWarnings"
            when: _numErrors == 0 && _numWarnings == 0

            QQ.PropertyChanges {
                target: overviewId
                visible: false
            }
        },

        QQ.State {
            name: "hasErrorOrWarning"
            QQ.PropertyChanges {
                target: overviewId
                visible: true
            }
        },

        QQ.State {
            name: "hasError"
            extend: "hasErrorOrWarning"

            QQ.PropertyChanges {
                target: borderRectangle
                border.color: "#930100"
                color: "#FAB8B9"
            }
        },

        QQ.State {
            name: "hasWarning"
            extend: "hasErrorOrWarning"


            QQ.PropertyChanges {
                target: borderRectangle
                border.color: "#E06841"
                color: "#FFFDBC"
            }
        },

        QQ.State {
            name: "onlyErrors"
            when: _numErrors > 0 && _numWarnings == 0
            extend: "hasError"
            QQ.PropertyChanges {
                target: errorText
                text: "There are <b>" + _numErrors + " errors</b>"
            }
        },

        QQ.State {
            name: "onlyWarnings"
            when: _numErrors == 0 && _numWarnings > 0
            extend: "hasWarning"

            QQ.PropertyChanges {
                target: errorText
                text: "There are " + _numWarnings + " warnings"
            }
        },

        QQ.State {
            name: "ErrorsAndWarnings"
            when: _numErrors > 0 && _numWarnings > 0
            extend: "hasError"


            QQ.PropertyChanges {
                target: errorText
                text: "There are <b>" + _numErrors + " errors</b> and " + _numWarnings + " warnings"
            }
        }
    ]
}

