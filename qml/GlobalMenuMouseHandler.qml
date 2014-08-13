/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

/**
    This class ketch mouse events in the main window.
  */
MouseArea {
    anchors.fill: parent

    property var currentRootContextMenu: null

    visible: currentRootContextMenu !== null;

    function closeOpenMenu() {
        if(currentRootContextMenu !== null) {
            currentRootContextMenu.visible = false
            currentRootContextMenu = null
        }
    }

    onPressed: {
        closeOpenMenu()
        mouse.accepted = false
    }


}
