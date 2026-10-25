pragma ComponentBehavior: Bound

import QtQuick as QQ
import cavewherelib
import QtQuick.Layouts
import QtQuick.Controls as QC

QQ.Loader {
    id: loaderId

    property ErrorListModel errors
    required property RoundButton errorIcon
    // Offers "ignore all similar" across this trip
    property Trip trip: null

    //This allows qml testcases to work correctly
    property string quoteBoxObjectName

    sourceComponent: visible ? popupComponent : null

    QQ.Component {
        id: popupComponent

        // Parented to the icon so a press on the icon itself leaves the popup
        // open and the icon's own click toggles it shut
        QC.Popup {
            id: popupId

            parent: loaderId.errorIcon
            x: loaderId.errorIcon.width * 0.5 + errorQuoteBox.bodyRect.x
            y: loaderId.errorIcon.height
            padding: 0
            modal: false
            dim: false
            closePolicy: QC.Popup.CloseOnEscape | QC.Popup.CloseOnPressOutsideParent
            visible: true
            background: null

            onClosed: loaderId.errorIcon.checked = false

            contentItem: QQ.Item {
                implicitWidth: errorQuoteBox.bodyRect.width
                implicitHeight: errorQuoteBox.bodyRect.height

                QuoteBox {
                    id: errorQuoteBox
                    objectName: loaderId.quoteBoxObjectName

                    x: -errorQuoteBox.bodyRect.x

                    ColumnLayout {
                        QQ.Repeater {
                            id: repeaterId
                            model: loaderId.errors === null ? 0 : loaderId.errors
                            delegate: ErrorRow {
                                id: delegateId
                                required property int index

                                onIgnoreToggled: (ignored) => {
                                    loaderId.errors.setData(loaderId.errors.index(delegateId.index, 0),
                                                            ignored,
                                                            ErrorListModel.SuppressedRole)
                                }
                            }
                        }

                        // One link per warning kind. A cell's warnings each carry their
                        // own errorTypeId, so one link per row is one link per kind.
                        QQ.Repeater {
                            model: loaderId.errors === null || loaderId.trip === null ? 0 : loaderId.errors
                            delegate: LinkText {
                                id: ignoreSimilarId
                                objectName: "ignoreSimilar"

                                required property int errorType
                                required property int errorTypeId
                                required property bool suppressed

                                // warningCountForTypeId has no notify signal; reading the
                                // trip's counts re-runs this whenever they move
                                readonly property int similarCount: {
                                    loaderId.trip.errorModel.warningCount
                                    loaderId.trip.errorModel.ignoredWarningCount
                                    return loaderId.trip.warningCountForTypeId(ignoreSimilarId.errorTypeId)
                                }

                                visible: ignoreSimilarId.errorType === CwError.Warning
                                         && !ignoreSimilarId.suppressed
                                         && ignoreSimilarId.errorTypeId !== 0
                                text: qsTr("Ignore all similar warnings in this trip (%1)").arg(ignoreSimilarId.similarCount)

                                onClicked: loaderId.trip.ignoreAllWarnings([ignoreSimilarId.errorTypeId])
                            }
                        }
                    }
                }
            }
        }
    }
}
