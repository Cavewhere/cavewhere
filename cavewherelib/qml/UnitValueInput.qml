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
    property alias unitReadOnly: unitInput.readOnly
    property int defaultUnit
    property alias unitModel: unitInput.unitModel
    property alias validator: clickInput.validator
    //property bool useCustomUnitModel: false  //Allows you use only subsection of the units

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

        unitModel: {
            if(itemId.unitValue !== null) {
                return itemId.unitValue.unitNames;
            } else {
                return null
            }
        }

        // A binding, never an assignment. The displayed unit is derived from
        // unitValue, so it has to keep tracking it — this used to be refreshed by
        // writing unitInput.unit from a function, which overwrote the binding on
        // the first run and froze the display. Anything that moved the unit from
        // C++ afterward (a trip switching survey units, say) never showed up.
        unit: {
            if(itemId.unitValue === null) { return itemId.defaultUnit }
            let index = privateData.valueToCustomUnits[itemId.unitValue.unit]
            return index === undefined ? itemId.defaultUnit : index
        }

        onNewUnit: function(unit) {
            if(itemId.unitValue !== null) {
                itemId.unitValue.unit = privateData.customUnitsToValue[unit]
            }
        }
    }

    QQ.QtObject {
        id: privateData

        // Both maps are rebuilt whole rather than written element by element:
        // QML sees the reassignment, not an in-place edit, so the unit binding
        // above re-evaluates when the model changes.
        readonly property var customUnitsToValue: {
            let map = []
            if(itemId.unitValue === null || !itemId.unitModel) { return map }
            for(let i = 0; i < itemId.unitModel.length; i++) {
                map[i] = itemId.unitValue.toUnitType(itemId.unitModel[i])
            }
            return map
        }

        readonly property var valueToCustomUnits: {
            let map = []
            for(let i = 0; i < privateData.customUnitsToValue.length; i++) {
                map[privateData.customUnitsToValue[i]] = i
            }
            return map
        }
    }
}
