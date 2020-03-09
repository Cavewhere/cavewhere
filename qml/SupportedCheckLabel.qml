import QtQuick 2.0 as QQ
import QtQuick.Controls 2.12 as QC
import QtQuick.Layouts 1.12

ColumnLayout {
    id: rootId
    property bool supported: true
    property alias using: checkboxId.using
    property string text: ""
    property alias helpText: checkboxId.helpText
    property alias helpIcon: checkboxId.helpIcon

    RowLayout {
        QQ.Image {
            id: stopId
            source: "qrc:/icons/stopSignError.png"
            width: 16
            height: 16
            visible: !supported
        }

        QQ.Image {
            id: goodId
            source: "qrc:/icons/good.png"
            width: stopId.width
            height: stopId.height
            visible: supported
        }

        Text {
            id: textId

            text: {
                var supportText = supported ? "supported" : "unsupported"
                return rootId.text + " is " + supportText;
            }
        }
    }

    CheckBoxWithInfo {
        id: checkboxId
        leftSpace: 16
        text: rootId.text
        checkboxEnabled: supported
    }

//    RowLayout {
//        QQ.Item {
//            width: 16
//            height: 1
//        }

//        InformationButton {
//            showItemOnClick: helpAreaId
//        }

//        QC.CheckBox {
//            text: "Use " + rootId.text
//            checked: using
//            onCheckedChanged: {
//                using = checked;
//            }
//        }
//    }

//    HelpArea {
//        id: helpAreaId
//        Layout.fillWidth: true
//    }
}
