// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0

MenuItem {
    id: menuId

    default property alias children: subMenu.children
    menu: true

    ContextMenu {
        id: subMenu
        visible: {
            if(menuId.parentContextMenu.visible) {
                return selected
            } else {
                selected = false
                return false;
            }
        }

        onVisibleChanged: {
            if(visible) {
                parent = menuId
                var globalPoint = menuId.mapToItem(null, menuId.parent.width, 0)
                x = globalPoint.x + subMenu.marginWidth
                y = globalPoint.y - subMenu.marginHeight
                parent = globalMenuMouseHandler
            }
        }
    }


}
