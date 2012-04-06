import QtQuick 1.1
//import QtDesktop 0.1

Item {
    id: unitInput
    property variant unitModel
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
        text: " " + numeratorMenu.selectedText

        MouseArea {
            anchors.fill: parent

            onClicked: {
                numeratorMenu.visible = true
            }
        }
    }

    // TODO: Put unit selection back in
//    ContextMenu {
//        id: numeratorMenu
//        model: listModel
//        selectedIndex: unitInput.unit
//        onSelectedIndexChanged: newUnit(selectedIndex)
//    }

    ListModel {
        id: listModel
    }

    onUnitModelChanged: {
        listModel.clear()
        for(var unit in unitModel) {
            listModel.append({"text": unitModel[unit]})
        }
        numeratorMenu.rebuildMenu()
        numeratorMenu.selectedIndex = unitInput.unit
    }
}
