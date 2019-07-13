import QtQuick 2.0
import QtQuick.Controls 2.5

UnitBaseItem {

    width: comboBoxId.width
    height: comboBoxId.height

    ComboBox {
        id: comboBoxId
        model: unitModel
        enabled: !readOnly
        currentIndex: unit

        onCurrentIndexChanged: {
            newUnit(currentIndex)
        }
    }
}
