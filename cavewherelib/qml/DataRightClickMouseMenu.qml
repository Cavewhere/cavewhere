import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
QQ.Item {
    id: root

    property point clickPos
    property RemoveAskBox removeChallenge
    required property int row
    required property string name

    // Optional — when non-empty, the Declination submenu acts on every
    // calibration in the list. The caller decides scope: a single row, or
    // the whole selection when the right-clicked row is part of it.
    property list<TripCalibration> tripCalibrations: []

    function showMenu(x, y) {
        clickPos = Qt.point(x, y)
        rightClickMenu.popup()
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

        QC.Menu {
            id: declinationSubmenu
            objectName: "declinationSubmenu"
            title: "Declination"
            // Submenu disables itself on pages with no calibrations wired so
            // the regular remove menu doesn't grow a useless stub.
            enabled: root.tripCalibrations.length > 0

            QC.MenuItem {
                objectName: "declinationAutoMenuItem"
                text: "Auto"
                checkable: true
                // A mixed selection (some auto, some manual) leaves both
                // items unchecked, doubling as a tri-state cue.
                checked: root.tripCalibrations.length > 0
                         && root.tripCalibrations.every(c => c.autoDeclination)
                enabled: root.tripCalibrations.length > 0
                onTriggered: root.tripCalibrations.forEach(c => c.autoDeclination = true)
            }

            QC.MenuItem {
                objectName: "declinationManualMenuItem"
                text: "Manual"
                checkable: true
                checked: root.tripCalibrations.length > 0
                         && root.tripCalibrations.every(c => !c.autoDeclination)
                enabled: root.tripCalibrations.length > 0
                onTriggered: root.tripCalibrations.forEach(c => c.autoDeclination = false)
            }
        }
    }
}
