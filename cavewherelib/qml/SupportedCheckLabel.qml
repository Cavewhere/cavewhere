import QtQuick.Layouts

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
        checkboxEnabled: rootId.supported
    }
}
