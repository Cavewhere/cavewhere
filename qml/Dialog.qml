import QtQuick 2.0

/**
  This shows a dialog in cavewhere

  This shuold probably be removed
  */
Rectangle {

    property bool fillParent: true //If true, this will try to file the root view

    parent: globalDialogHandler
    anchors.fill: parent

}
