pragma ComponentBehavior: Bound

import QtQuick as QQ
import cavewherelib
import QtQuick.Layouts
import QtQuick.Controls as QC

QQ.Loader {
    id: loaderId

    property ErrorListModel errors
    required property QQ.Item errorIcon

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

                        Text {
                            objectName: "errorText"
                            text: delegateId.message
                            font.strikeout: delegateId.suppressed
                        }
                    }
                }
            }
        }
    }
}
