/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ

QQ.Rectangle {
    id: rect
    color: "red"
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
