import QtQuick.Controls

UnitBaseItem {
    id: itemId

    width: comboBoxId.width
    height: comboBoxId.height

    ComboBox {
        id: comboBoxId
        model: itemId.unitModel
        enabled: !itemId.readOnly
        currentIndex: itemId.unit

        onCurrentIndexChanged: {
            itemId.newUnit(currentIndex)
        }
    }
}
