import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
QQ.Item {
    id: root

    property point clickPos
    property RemoveAskBox removeChallenge
    required property int row
    required property string name

    function showMenu(x, y) {
        clickPos = Qt.point(x, y)
        rightClickMenu.popup()
    }

    QQ.TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (eventPoint) => root.showMenu(eventPoint.position.x,
                                                eventPoint.position.y)
    }

    QQ.TapHandler {
        onLongPressed: root.showMenu(point.position.x, point.position.y)
    }

    QC.Menu {
        id: rightClickMenu
        QC.MenuItem {
            text: "Remove " + root.name

            onTriggered: {
                var pos = root.mapToItem(root.removeChallenge.parent,
                                         root.clickPos.x,
                                         root.clickPos.y)
                root.removeChallenge.x = pos.x
                root.removeChallenge.y = pos.y

                root.removeChallenge.indexToRemove = root.row
                root.removeChallenge.removeName = root.name
                root.removeChallenge.show()
            }
        }
    }
}
