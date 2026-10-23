import QtQuick as QQ
import cavewherelib
import QtQuick.Layouts
import QtQuick.Controls as QC
QQ.Item {
    id: overviewId
    objectName: "surveyErrorOverview"

    property Trip trip: null

    property int _numWarnings: trip.errorModel.warningCount
    property int _numErrors: trip.errorModel.fatalCount
    property int _numAccepted: trip.errorModel.suppressedWarningCount

    readonly property int iconSize: 16
    readonly property int horizontalPadding: 20
    readonly property int verticalPadding: 10

    // Stopgap until the accepted-warnings section under the badge carries the count
    function acceptedSuffix() {
        return overviewId._numAccepted > 0 ? qsTr(" · %1 accepted").arg(overviewId._numAccepted) : ""
    }

    implicitWidth: borderRectangle.width
    implicitHeight: borderRectangle.height

    visible: false

    QQ.Rectangle {
        id: borderRectangle

        border.width: 1
        radius: 3
        width: layoutId.width + overviewId.horizontalPadding
        height: layoutId.height + overviewId.verticalPadding

        // The caret is a hint; the whole badge opens the same menu
        QQ.TapHandler {
            enabled: menuButtonId.visible
            onTapped: (eventPoint) => {
                const local = menuButtonId.mapFromItem(borderRectangle, eventPoint.position)
                if (!menuButtonId.contains(local)) {
                    menuButtonId.clicked()
                }
            }
        }

        RowLayout {
            id: layoutId
            anchors.centerIn: parent

            QQ.Image {
                id: stopImage
                source: "qrc:icons/svg/stopSignError.svg"
                visible: overviewId._numErrors > 0
                sourceSize: Qt.size(overviewId.iconSize, overviewId.iconSize)
            }

            QQ.Image {
                id: warningImage
                source: "qrc:icons/svg/warning.svg"
                visible: overviewId._numWarnings > 0
                sourceSize: Qt.size(overviewId.iconSize, overviewId.iconSize)
            }

            QQ.Image {
                id: acceptedImage
                source: "qrc:icons/svg/warningAccepted.svg"
                visible: overviewId._numErrors === 0 && overviewId._numWarnings === 0 && overviewId._numAccepted > 0
                sourceSize: Qt.size(overviewId.iconSize, overviewId.iconSize)
            }

            QC.Label {
                id: errorText
                objectName: "overviewText"
            }

            ContextMenuButton {
                id: menuButtonId
                objectName: "tripWarningsMenuButton"

                visible: overviewId._numWarnings > 0 || overviewId._numAccepted > 0
                focusPolicy: Qt.NoFocus
                menu: tripWarningsMenuComponent
            }
        }
    }

    QQ.Component {
        id: tripWarningsMenuComponent

        TripWarningsMenu {
            trip: overviewId.trip
        }
    }

    states: [
        QQ.State {
            name: "noErrorsOrWarnings"
            when: overviewId._numErrors == 0 && overviewId._numWarnings == 0 && overviewId._numAccepted == 0

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

        // Only accepted warnings remain: the badge stays so the menu can
        // un-accept them, but it reads as resolved
        QQ.State {
            name: "onlyAccepted"
            when: overviewId._numErrors == 0 && overviewId._numWarnings == 0 && overviewId._numAccepted > 0
            extend: "hasErrorOrWarning"

            QQ.PropertyChanges {
                borderRectangle {
                    border.color: Theme.border
                    color: Theme.surfaceMuted
                }
                errorText {
                    text: overviewId._numAccepted === 1 ? qsTr("1 accepted warning")
                                                         : qsTr("%1 accepted warnings").arg(overviewId._numAccepted)
                }
            }
        },

        QQ.State {
            name: "onlyErrors"
            when: overviewId._numErrors > 0 && overviewId._numWarnings == 0
            extend: "hasError"
            QQ.PropertyChanges {
                errorText {
                    text: "There are <b>" + overviewId._numErrors + " errors</b>" + overviewId.acceptedSuffix()
                }
            }
        },

        QQ.State {
            name: "onlyWarnings"
            when: overviewId._numErrors == 0 && overviewId._numWarnings > 0
            extend: "hasWarning"

            QQ.PropertyChanges {
                errorText {
                    text: "There are " + overviewId._numWarnings + " warnings" + overviewId.acceptedSuffix()
                }
            }
        },

        QQ.State {
            name: "ErrorsAndWarnings"
            when: overviewId._numErrors > 0 && overviewId._numWarnings > 0
            extend: "hasError"


            QQ.PropertyChanges {
                errorText {
                    text: "There are <b>" + overviewId._numErrors + " errors</b> and " + overviewId._numWarnings + " warnings" + overviewId.acceptedSuffix()
                }
            }
        }
    ]
}
