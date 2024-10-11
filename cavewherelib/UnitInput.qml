/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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
        color: readOnly ? "black" : pallete.inputTextColor
        text: unitModel !== null && typeof(unitModel) !== 'undefined' ? " " + unitModel[menuId.selectedIndex] : "";

        QQ.MouseArea {
            anchors.fill: parent
            enabled: !readOnly

            onClicked: {
                menuId.popup()
            }
        }

        Controls.Menu {
            id: menuId

            property int selectedIndex: unitInput.unit

            Instantiator {
                model: unitModel
                Controls.MenuItem {
                    text: modelData; //unitModel[index]
                    onTriggered: unitInput.newUnit(index)
                }
                onObjectAdded: menuId.insertItem(index, object)
                onObjectRemoved: menuId.removeItem(object)
            }
        }
    }
}
