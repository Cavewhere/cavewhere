import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
QQ.MouseArea {
    id: rightClickMouseArea
    acceptedButtons: Qt.RightButton

    property point clickPos;
    property RemoveAskBox removeChallenge
    required property int row
    required property string name

    onClicked: (mouse) => {
        clickPos = Qt.point(mouse.x, mouse.y)
        rightClickMenu.popup()
        mouse.accepted = false
    }

    QC.Menu {
        id: rightClickMenu
        QC.MenuItem {
            text: "Remove " + rightClickMouseArea.name

            onTriggered: {
                var pos = rightClickMouseArea.mapToItem(rightClickMouseArea.removeChallenge.parent,
                                                        rightClickMouseArea.clickPos.x,
                                                        rightClickMouseArea.clickPos.y)
                rightClickMouseArea.removeChallenge.x = pos.x
                rightClickMouseArea.removeChallenge.y = pos.y

                rightClickMouseArea.removeChallenge.indexToRemove = rightClickMouseArea.row
                rightClickMouseArea.removeChallenge.removeName = rightClickMouseArea.name
                rightClickMouseArea.removeChallenge.show()
            }
        }
    }
}
