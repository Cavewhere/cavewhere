// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

/**
    This class ketch mouse events in the main window.
  */
MouseArea {
    anchors.fill: parent

    property var currentRootContextMenu: null

    visible: true;

    onPressed: {
        if(currentRootContextMenu !== null) {
            currentRootContextMenu.visible = false
            currentRootContextMenu = null
        }
        mouse.accepted = false
    }
}
