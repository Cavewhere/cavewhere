import QtQuick 1.0

Item {
    id: buttonGroup

    default property alias content: listOfButton.children
    property int currentItemIndex: 0

    onCurrentItemIndexChanged: setVisiblities()
    Component.onCompleted: set

    function setVisiblities() {
        for(var i = 0; i < listOfButton.children.length; i++) {
            listOfButton.children[i].visible = (i == currentItemIndex ? true : false);
        }
    }


    Row {
        id: button

        Repeater {
            model:  listOfButton.children.length

            delegate: SideBarButton {
                onTroggledChanged: if(troggled) { currentItemIndex = index; }
            }
        }

    }

    Item {
        id: listOfButton
    }
}
