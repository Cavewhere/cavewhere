// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0
import "Utils.js" as Utils

Row {
    property Length length
    property alias valueVisible: clickInput.visible
    property alias valueReadOnly: clickInput.readOnly

    ClickTextInput {
        id: clickInput
        text: length !== null ? Utils.fixed(length.value, 2) : ""
        onFinishedEditting: if(length !== null) { length.value = newText }
    }

    UnitInput {
        id: unitInput
        unitModel: {
            if(length !== null) {
                return length.unitNames;
            } else {
                return null
            }
        }
        unit: {
            if(length !== null) {
                return length.unit;
            } else {
                return Units.Unitless
            }
        }
        onNewUnit: {
            if(length !== null) {
                length.unit = unit
            }
        }
    }
}
