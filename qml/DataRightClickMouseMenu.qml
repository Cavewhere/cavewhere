import QtQuick 2.0
import QtQuick.Controls 1.0

MouseArea {
    id: rightClickMouseArea
    acceptedButtons: Qt.RightButton

    property point clickPos;
    property RemoveAskBox removeChallenge

    onClicked: {
        clickPos = Qt.point(mouse.x, mouse.y)
        rightClickMenu.popup()
        mouse.accepted = false
    }

    Menu {
        id: rightClickMenu
        MenuItem {
            text: "Remove " + styleData.value.name

            onTriggered: {
                var pos = rightClickMouseArea.mapToItem(removeChallenge.parent,
                                                        rightClickMouseArea.clickPos.x,
                                                        rightClickMouseArea.clickPos.y)
                removeChallenge.x = pos.x
                removeChallenge.y = pos.y
                removeChallenge.index = styleData.row
                removeChallenge.removeName = styleData.value.name
                removeChallenge.show()
            }
        }
    }
}
