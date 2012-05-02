// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0

MenuWindow {
    id: menuWindow

    default property alias children: childItems.children
    width: childItems.width
    height: childItems.height

    Column {
        id: childItems

    }
}
