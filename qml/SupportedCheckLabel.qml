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

    SupportedLabel {
        supported: rootId.supported
        text: rootId.text
    }

    CheckBoxWithInfo {
        id: checkboxId
        leftSpace: 16
        text: rootId.text
        checkboxEnabled: supported
    }
}
