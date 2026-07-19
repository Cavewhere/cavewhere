import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib
import "Utils.js" as Utils

RowLayout {
    id: rootId

    property string label
    property UnitValue unitValue: null
    property var unitModel

    spacing: Theme.delegatePadding

    QC.Label {
        text: rootId.label
    }

    SelectableValue {
        id: valueInput
        text: rootId.unitValue !== null ? Utils.fixed(rootId.unitValue.value, 2) : ""
        font.pixelSize: Theme.fontSizeUI
    }

    UnitValueInput {
        id: unitValueInputId
        unitValue: rootId.unitValue
        unitModel: rootId.unitModel
        valueVisible: false
        valueReadOnly: true
    }
}
