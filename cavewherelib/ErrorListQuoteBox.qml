pragma ComponentBehavior: Bound

import QtQuick as QQ
import cavewherelib
import QtQuick.Layouts
// import QtQuick.Controls as QC

QQ.Loader {
    id: loaderId

    property ErrorListModel errors
    required property Button errorIcon

    sourceComponent: visible ? quoteBoxCompent : null


    QQ.Component {
        id: quoteBoxCompent

        QuoteBox {
            id: errorQuoteBox
            z: 10

            parent: RootPopupItem

            function errorImageSource(errorType) {
                switch(errorType) {
                case CwError.Fatal:
                    return "qrc:icons/stopSignError.png";
                case CwError.Warning:
                    return "qrc:icons/warning.png"
                default:
                    return "";
                }
            }

            pointAtObject: loaderId.errorIcon
            pointAtObjectPosition: Qt.point(loaderId.errorIcon.width * 0.5, loaderId.errorIcon.height)
            triangleEdge: Qt.TopEdge

            ColumnLayout {
                QQ.Repeater {
                    id: repeaterId
                    model: loaderId.errors === null ? 0 : loaderId.errors
                    delegate:
                        RowLayout {
                        id: delegateId
                        property int type
                        property string message
                        property bool suppressed
                        property int index

                        QQ.Image {
                            source: errorQuoteBox.errorImageSource(delegateId.type)
                        }

                        CheckBox {
                            id: checkBoxId
                            checked: delegateId.suppressed
                            onCheckedChanged: {
                                //SuppressWarning
                                if(delegateId.suppressed !== checked) {
                                    delegateId.suppressed = checked
                                    loaderId.errors.setData(delegateId.index, delegateId.suppressed, "suppressed")
                                }
                            }
                            visible: delegateId.type === CwError.Warning
                        }

                        Text {
                            text: delegateId.message
                            font.strikeout: delegateId.suppressed
                        }
                    }

                }
            }
        }
    }
}
