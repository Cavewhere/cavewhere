import QtQuick as QQ
import cavewherelib
import QtQuick.Layouts 1.1

QQ.Item {
    id: overviewId

    property Trip trip: null

    // property var _chunkToWatch: [] //Array of chunks to watch
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
                visible: overviewId._numErrors > 0
            }

            QQ.Image {
                id: warningImage
                source: "qrc:icons/warning.png"
                visible: overviewId._numWarnings > 0
            }

            Text {
                id: errorText
            }
        }


    }

    states: [
        QQ.State {
            name: "noErrorsOrWarnings"
            when: overviewId._numErrors == 0 && overviewId._numWarnings == 0

            QQ.PropertyChanges {
                overviewId {
                    visible: false
                }
            }
        },

        QQ.State {
            name: "hasErrorOrWarning"
            QQ.PropertyChanges {
                overviewId {
                    visible: true
                }
            }
        },

        QQ.State {
            name: "hasError"
            extend: "hasErrorOrWarning"

            QQ.PropertyChanges {
                borderRectangle {
                    border.color: "#930100"
                    color: "#FAB8B9"
                }
            }
        },

        QQ.State {
            name: "hasWarning"
            extend: "hasErrorOrWarning"


            QQ.PropertyChanges {
                borderRectangle {
                    border.color: "#E06841"
                    color: "#FFFDBC"
                }
            }
        },

        QQ.State {
            name: "onlyErrors"
            when: overviewId._numErrors > 0 && overviewId._numWarnings == 0
            extend: "hasError"
            QQ.PropertyChanges {
                errorText {
                    text: "There are <b>" + overviewId._numErrors + " errors</b>"
                }
            }
        },

        QQ.State {
            name: "onlyWarnings"
            when: overviewId._numErrors == 0 && overviewId._numWarnings > 0
            extend: "hasWarning"

            QQ.PropertyChanges {
                errorText {
                    text: "There are " + overviewId._numWarnings + " warnings"
                }
            }
        },

        QQ.State {
            name: "ErrorsAndWarnings"
            when: overviewId._numErrors > 0 && overviewId._numWarnings > 0
            extend: "hasError"


            QQ.PropertyChanges {
                errorText {
                    text: "There are <b>" + overviewId._numErrors + " errors</b> and " + overviewId._numWarnings + " warnings"
                }
            }
        }
    ]
}

