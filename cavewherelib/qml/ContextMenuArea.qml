import QtQuick as QQ
import QtQuick.Controls as QC

// Generic right-click / long-press context-menu frame. Owns the input
// plumbing (TapHandlers, click-position capture, popup) and an empty
// QC.Menu. Consumers add MenuItems / submenus as direct children — they
// land in the inner Menu via the default-property alias. Holds no opinion
// about menu content; page-specific items live in the consumer.
QQ.Item {
    id: root

    default property alias menuItems: rightClickMenu.contentData

    // Click position in this Item's coordinates, captured at tap time so
    // popup-positioning logic (e.g. RemoveMenuItem mapping to a dialog)
    // sees where the user actually clicked, not where the menu opens.
    property point clickPos

    // Exposes the inner QC.Menu so callers / tests can reach itemAt(),
    // count, opened, etc. — QC.Menu's children live in `contentData`,
    // which findChild can't traverse before the popup is materialized.
    readonly property alias menu: rightClickMenu

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

    // Touch-only so mouse left-clicks pass through to siblings (e.g.
    // LinkText delegates underneath).
    QQ.TapHandler {
        acceptedDevices: QQ.PointerDevice.TouchScreen
        onLongPressed: root.showMenu(point.position.x, point.position.y)
    }

    QC.Menu {
        id: rightClickMenu
    }
}
