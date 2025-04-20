/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib
import QtQuick.Controls as QC
import QtQuick

QC.RoundButton {
    id: moreButton

    // required property QC.Menu menu
    required property Component menu

    icon.source: "qrc:/twbs-icons/icons/caret-down-fill.svg"
    implicitWidth: 20
    implicitHeight: 20
    radius: 0

    Loader {
        id: menuLoader
        objectName: "menuLoader"
        sourceComponent: menu
        active: false
        anchors.fill: parent
    }

    onClicked: {
        menuLoader.active = true
        menuLoader.item.popup();
    }
}
