/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick as QQ
import cavewherelib
import "Utils.js" as Utils

QQ.Row {
    id: itemId

    property UnitValue unitValue: null
    property alias valueVisible: clickInput.visible
    property alias valueReadOnly: clickInput.readOnly
    property int defaultUnit
    property alias unitModel: unitInput.unitModel
    property alias validator: clickInput.validator
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

        unitInput.unit = unitInput.updateUnit();
    }

    ClickTextInput {
        id: clickInput
        text: itemId.unitValue !== null ? Utils.fixed(itemId.unitValue.value, 2) : ""
        onFinishedEditting: (newText) => {
                                if(itemId.unitValue !== null) { itemId.unitValue.value = newText }
                            }
    }

    UnitInput {
        id: unitInput
        objectName: "unitInput"

        function updateUnit() {
            if(itemId.unitValue !== null && privateData.customUnitsToValue.length > 0) {
                return privateData.valueToCustomUnits[unitValue.unit];
            } else {
                return defaultUnit
            }
        }

        unitModel: {
            if(itemId.unitValue !== null) {
                return itemId.unitValue.unitNames;
            } else {
                return null
            }
        }

        unit: updateUnit()

        onNewUnit: function(unit) {
            if(itemId.unitValue !== null) {
                itemId.unitValue.unit = privateData.customUnitsToValue[unit]
                unitInput.unit = updateUnit()
            }
        }
    }

    QQ.QtObject {
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

    QQ.Component.onCompleted: {
        updateMap()
    }
}
