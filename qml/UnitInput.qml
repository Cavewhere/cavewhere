/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0

Item {
    id: unitInput
    property var unitModel
    property int unit

    signal newUnit(int unit)

    width: textArea.width
    height: textArea.height

    onUnitChanged: {
        numeratorMenu.selectedIndex = unitInput.unit
    }

    Pallete {
        id: pallete
    }

    Text {
        id: textArea
        color: pallete.inputTextColor
        text: unitModel !== null ? " " + unitModel[numeratorMenu.selectedIndex] : "";

        MouseArea {
            anchors.fill: parent

            onClicked: {
                numeratorMenu.popupOnTopOf(textArea, 0, textArea.height)
            }
        }

        ContextMenu {
            id: numeratorMenu
            property int selectedIndex: unitInput.unit

            Repeater {
                model: unitModel

                delegate: MenuItem {
                    text: modelData
                    onTriggered: unitInput.newUnit(index)
                }
            }
        }
    }
}
