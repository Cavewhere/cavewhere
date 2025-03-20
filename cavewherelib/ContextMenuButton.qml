/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib
import QtQuick.Controls as QC

QC.RoundButton {
    id: moreButton

    required property QC.Menu menu

    icon.source: "qrc:/twbs-icons/icons/caret-down-fill.svg"
    // icon.width: 15
    // icon.height: 15
    implicitWidth: 20
    implicitHeight: 20
    radius: 0

    onClicked: {
        menu.popup()
    }
}
