import QtQuick as QQ
import cavewherelib
import QtQuick.Layouts
import QtQuick.Controls as QC
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
                source: "qrc:icons/svg/stopSignError.svg"
                visible: overviewId._numErrors > 0
                sourceSize: Qt.size(16, 16)
            }

            QQ.Image {
                id: warningImage
                source: "qrc:icons/svg/warning.svg"
                visible: overviewId._numWarnings > 0
                sourceSize: Qt.size(16, 16)
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
                    border.color: Theme.danger
                    color: Theme.danger
                }
            }
        },

        QQ.State {
            name: "hasWarning"
            extend: "hasErrorOrWarning"


            QQ.PropertyChanges {
                borderRectangle {
                    border.color: Theme.warning
                    color: Theme.warning
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
