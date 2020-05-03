import QtQuick 2.0 as QQ
import Cavewhere 1.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.12 as QC

QQ.Loader {

    property ErrorListModel errors

    sourceComponent: visible ? quoteBoxCompent : null


    QQ.Component {
        id: quoteBoxCompent

        QuoteBox {
            id: errorQuoteBox
            z: 10

            parent: rootPopupItem

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

            pointAtObject: errorIcon
            pointAtObjectPosition: Qt.point(errorIcon.width * 0.5, errorIcon.height)
            triangleEdge: Qt.TopEdge

            ColumnLayout {
                QQ.Repeater {
                    id: repeaterId
                    model: errors === null ? 0 : errors
                    delegate:
                        RowLayout {

                        QQ.Image {
                            source: errorQuoteBox.errorImageSource(type)
                        }

                        CheckBox {
                            checked: suppressed
                            onCheckedChanged: {
                                //SuppressWarning
                                if(suppressed !== checked) {
                                    suppressed = checked
                                    errors.setData(index, suppressed, "suppressed")
                                }
                            }
                            visible: type === CwError.Warning
                        }

                        Text {
                            text: message
                            font.strikeout: suppressed
                        }
                    }

                }
            }
        }
    }
}
