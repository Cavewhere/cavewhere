// import QtQuick 2.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0
import "Utils.js" as Utils

Row {
    property var unitValue: null
    property alias valueVisible: clickInput.visible
    property alias valueReadOnly: clickInput.readOnly
    property int defaultUnit
    property alias unitModel: unitInput.unitModel
    //property bool useCustomUnitModel: false  //Allows you use only subsection of the units

    function updateMap() {

        if(typeof unitModel === "undefined" ||
                typeof unitValue === "undefined") {
            return;
        }

        if(unitValue !== null && unitModel) {

            //Clear the privateData
            privateData.customUnitsToValue = []
            privateData.valueToCustomUnits = []

            for(var i = 0; i < unitModel.length; i++) {
                var type = unitValue.toUnitType(unitModel[i]);
                privateData.customUnitsToValue[i] = type;
                privateData.valueToCustomUnits[type] = i;
            }
        }
    }

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
            if(unitValue !== null && privateData.customUnitsToValue.length > 0) {
                return privateData.valueToCustomUnits[unitValue.unit];
            } else {
                return defaultUnit
            }
        }

        onNewUnit: {
            if(unitValue !== null) {
                unitValue.unit = privateData.customUnitsToValue[unit]
            }
        }
    }

    QtObject {
        id: privateData
        property var customUnitsToValue: []
        property var valueToCustomUnits: []
    }

    onUnitValueChanged: {
        updateMap()
    }

    onUnitModelChanged: {
        updateMap()
    }

    Component.onCompleted: {
        updateMap()
    }
}
