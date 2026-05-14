import QtQuick
import QtQuick.Controls as QC
import cavewherelib
/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5

QC.Label {
    id: labelText
    color: Theme.textInverse
    style: Text.Outline;
    styleColor: Theme.text
    font.pixelSize: Theme.fontSizeUI

    opacity: 0.0

    function fadeInIfVisible() {
        if(visible) {
            opacity = 0.0
            fadeIn.restart()
        }
    }

    Component.onCompleted: fadeInIfVisible()
    onVisibleChanged: fadeInIfVisible()

    NumberAnimation {
        id: fadeIn
        target: labelText
        property: "opacity"
        from: 0.0
        to: 1.0
        duration: 1000
        easing.type: Easing.OutQuad
    }
}
