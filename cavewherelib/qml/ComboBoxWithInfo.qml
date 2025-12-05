import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: rootId

    property alias model: comboBoxId.model
    property alias currentIndex: comboBoxId.currentIndex
    property alias text: textId.text
    property alias helpText: helpAreaId.text

    RowLayout {
        InformationButton {
            showItemOnClick: helpAreaId
        }

        Text {
            id: textId
            enabled: rootId.enabled
        }

        QC.ComboBox {
            id: comboBoxId
            enabled: rootId.enabled
        }
    }

    HelpArea {
        id: helpAreaId
        Layout.fillWidth: true
    }


}
