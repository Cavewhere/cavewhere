import QtQuick 2.0 as QQ
import QtQuick.Controls 2.12 as QC
import QtQuick.Layouts 1.12
import cavewherelib

ColumnLayout {
    id: rootId

    property bool using: true
    property string text: ""
    property alias helpText: helpAreaId.text
    property alias helpIcon: helpAreaId.helpImageSource
    property alias leftSpace: spacerId.width
    property alias checkboxEnabled: checkBoxId.enabled

    RowLayout {

        QQ.Item {
            id: spacerId
            implicitWidth: 0
            implicitHeight: 1
            visible: width > 0
        }

        InformationButton {
            showItemOnClick: helpAreaId
        }

        QC.CheckBox {
            id: checkBoxId
            text: "Use " + rootId.text
            checked: rootId.using
            onCheckedChanged: {
                rootId.using = checked;
            }
        }
    }

    HelpArea {
        id: helpAreaId
        Layout.fillWidth: true
    }
}
