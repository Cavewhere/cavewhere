import QtQuick 2.0 as QQ
import QtQuick.Controls

QQ.MouseArea {
    id: rightClickMouseArea
    acceptedButtons: Qt.RightButton

    property point clickPos;
    property RemoveAskBox removeChallenge

    onClicked: (mouse) => {
        clickPos = Qt.point(mouse.x, mouse.y)
        rightClickMenu.popup()
        mouse.accepted = false
    }

    Menu {
        id: rightClickMenu
        MenuItem {
            // text: "Remove " + styleData.value.name
            text: "Remove " + "FIXME!"

            onTriggered: {
                var pos = rightClickMouseArea.mapToItem(rightClickMouseArea.removeChallenge.parent,
                                                        rightClickMouseArea.clickPos.x,
                                                        rightClickMouseArea.clickPos.y)
                rightClickMouseArea.removeChallenge.x = pos.x
                rightClickMouseArea.removeChallenge.y = pos.y
                //FIXME: below has been comment out
                // rightClickMouseArea.removeChallenge.indexToRemove = styleData.row
                // rightClickMouseArea.removeChallenge.removeName = styleData.value.name
                rightClickMouseArea.removeChallenge.show()
            }
        }
    }
}
