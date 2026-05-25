/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib
import QtQuick.Controls as QC
import QtQuick

RoundButton {
    id: moreButton

    // required property QC.Menu menu
    required property Component menu

    // Per-callsite override so page-level menus can use a hamburger glyph
    // while field-level dropdowns keep the caret default.
    property url iconSource: "qrc:/twbs-icons/icons/caret-down-fill.svg"

    icon.source: iconSource
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
