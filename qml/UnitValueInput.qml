// import QtQuick 2.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0
import "Utils.js" as Utils

Row {
    property var unitValue: null
    property alias valueVisible: clickInput.visible
    property alias valueReadOnly: clickInput.readOnly
    property int defaultUnit

    ClickTextInput {
        id: clickInput
        text: unitValue !== null ? Utils.fixed(unitValue.value, 2) : ""
        onFinishedEditting: if(unitValue !== null) { unitValue.value = newText }
    }

    UnitInput {
        id: unitInput
        unitModel: {
            if(unitValue !== null) {
                return unitValue.unitNames;
            } else {
                return null
            }
        }
        unit: {
            if(unitValue !== null) {
                return unitValue.unit;
            } else {
                return defaultUnit
            }
        }
        onNewUnit: {
            if(unitValue !== null) {
                unitValue.unit = unit
            }
        }
    }
}
