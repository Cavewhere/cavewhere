/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

Button {
    id: moreButton

    default property var menu

    iconSource: "qrc:/icons/moreArrowDown.png"
    iconSize: Qt.size(8, 8)
    height: 13
    width: 13
    radius: 0

    onClicked: {
        menu.popup()
    }
}
