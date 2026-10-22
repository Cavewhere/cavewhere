import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
QQ.Item {
    id: root

    property point clickPos
    property RemoveAskBox removeChallenge
    required property int row
    required property string name
    //! Built by the first right-click or long-press; null until then.
    readonly property QC.Menu menu: menuLoader.item as QC.Menu

    function showMenu(x, y) {
        clickPos = Qt.point(x, y)
        menuLoader.active = true
        root.menu.popup(root, x, y)
    }

    QQ.TapHandler {
        acceptedButtons: Qt.RightButton
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad
        onTapped: (eventPoint) => root.showMenu(eventPoint.position.x,
                                                eventPoint.position.y)
    }

    // Touch-only so mouse left-clicks pass through to the LinkText below.
    QQ.TapHandler {
        acceptedDevices: QQ.PointerDevice.TouchScreen
        onLongPressed: root.showMenu(point.position.x, point.position.y)
    }

    QQ.Loader {
        id: menuLoader
        active: false

        sourceComponent: QC.Menu {
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
}
