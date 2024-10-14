/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/
pragma ComponentBehavior: Bound

import QtQml 2.2
import QtQuick 2.2 as QQ
import QtQuick.Controls 2.0 as Controls

UnitBaseItem {
    id: unitInput

    width: textArea.width
    height: textArea.height

    onUnitChanged: {
        menuId.selectedIndex = unitInput.unit
    }

    Pallete {
        id: pallete
    }

    Text {
        id: textArea
        color: unitInput.readOnly ? "black" : pallete.inputTextColor
        text: unitInput.unitModel !== null && typeof(unitInput.unitModel) !== 'undefined' ? " " + unitInput.unitModel[menuId.selectedIndex] : "";

        QQ.MouseArea {
            anchors.fill: parent
            enabled: !unitInput.readOnly

            onClicked: {
                menuId.popup()
            }
        }

        Controls.Menu {
            id: menuId

            property int selectedIndex: unitInput.unit

            Instantiator {
                model: unitInput.unitModel
                Controls.MenuItem {
                    required property string modelData
                    required property int index

                    text: modelData; //unitModel[index]
                    onTriggered: unitInput.newUnit(index)
                }
                onObjectAdded: (index, object) => menuId.insertItem(index, object)
                onObjectRemoved: (index, object) => menuId.removeItem(object)
            }
        }
    }
}
