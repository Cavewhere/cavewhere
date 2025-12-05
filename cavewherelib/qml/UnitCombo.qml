import QtQuick.Controls as QC
import cavewherelib
UnitBaseItem {
    id: itemId

    width: comboBoxId.width
    height: comboBoxId.height

    QC.ComboBox {
        id: comboBoxId
        model: itemId.unitModel
        enabled: !itemId.readOnly
        currentIndex: itemId.unit

        onCurrentIndexChanged: {
            itemId.newUnit(currentIndex)
        }
    }
}
