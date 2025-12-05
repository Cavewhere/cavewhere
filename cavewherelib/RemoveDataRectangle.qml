/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

QQ.Rectangle {
    id: rect
    color: Theme.danger
    opacity: .5

    onVisibleChanged: {
        if(visible) {
            opacityAnimation.restart()
        }
    }

    QQ.NumberAnimation {
        id: opacityAnimation
        target: rect
        property: "opacity"
        from: 0.0
        to: 0.5
    }
}
