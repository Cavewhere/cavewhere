pragma ComponentBehavior: Bound

import QtQuick as QQ
import cavewherelib
import QtQuick.Layouts
import QtQuick.Controls as QC

QQ.Loader {
    id: loaderId

    property ErrorListModel errors
    required property QQ.Item errorIcon
    // Offers "accept all similar" across this trip
    property Trip trip: null

    //This allows qml testcases to work correctly
    property string quoteBoxObjectName

    sourceComponent: visible ? quoteBoxCompent : null

    QQ.Component {
        id: quoteBoxCompent

        QuoteBox {
            id: errorQuoteBox
            objectName: loaderId.quoteBoxObjectName
            z: 10

            parent: RootPopupItem

            function errorImageSource(errorType) {
                switch(errorType) {
                case CwError.Fatal:
                    return "qrc:icons/svg/stopSignError.svg";
                case CwError.Warning:
                    return "qrc:icons/svg/warning.svg"
                default:
                    return "";
                }
            }

            pointAtObject: loaderId.errorIcon
            pointAtObjectPosition: Qt.point(loaderId.errorIcon.width * 0.5, loaderId.errorIcon.height)
            // triangleEdge: Qt.TopEdge

            ColumnLayout {
                QQ.Repeater {
                    id: repeaterId
                    model: loaderId.errors === null ? 0 : loaderId.errors
                    delegate:
                        RowLayout {
                        id: delegateId
                        required property int errorType
                        required property string message
                        required property bool suppressed
                        required property int index

                        QQ.Image {
                            source: errorQuoteBox.errorImageSource(delegateId.errorType)
                            sourceSize: Qt.size(16, 16)
                        }

                        CheckBox {
                            id: checkBoxId
                            objectName: "suppress"
                            checked: delegateId.suppressed
                            onCheckedChanged: {
                                //SuppressWarning
                                if(delegateId.suppressed !== checked) {
                                    delegateId.suppressed = checked
                                    let index = loaderId.errors.index(delegateId.index, 0)
                                    loaderId.errors.setData(index, delegateId.suppressed, ErrorListModel.SuppressedRole)
                                }
                            }
                            visible: delegateId.errorType === CwError.Warning
                        }

                        QC.Label {
                            objectName: "errorText"
                            // Error messages quote user-typed names, so they are
                            // never markup — AutoText would guess otherwise for a
                            // station named A<b>B. ErrorListView's TextEdit is
                            // plain by default; this is the same contract.
                            textFormat: QC.Label.PlainText
                            text: delegateId.message
                            font.strikeout: delegateId.suppressed
                        }
                    }
                }

                // One link per warning kind. A cell's warnings each carry their
                // own errorTypeId, so one link per row is one link per kind.
                QQ.Repeater {
                    model: loaderId.errors === null || loaderId.trip === null ? 0 : loaderId.errors
                    delegate: LinkText {
                        id: acceptSimilarId
                        objectName: "acceptSimilar"

                        required property int errorType
                        required property int errorTypeId
                        required property bool suppressed

                        // warningCountForTypeId has no notify signal; reading the
                        // trip's counts re-runs this whenever they move
                        readonly property int similarCount: {
                            loaderId.trip.errorModel.warningCount
                            loaderId.trip.errorModel.suppressedWarningCount
                            return loaderId.trip.warningCountForTypeId(acceptSimilarId.errorTypeId)
                        }

                        visible: acceptSimilarId.errorType === CwError.Warning
                                 && !acceptSimilarId.suppressed
                                 && acceptSimilarId.errorTypeId !== 0
                        text: qsTr("Accept all similar warnings in this trip (%1)").arg(acceptSimilarId.similarCount)

                        onClicked: loaderId.trip.acceptAllWarnings([acceptSimilarId.errorTypeId])
                    }
                }
            }
        }
    }
}
