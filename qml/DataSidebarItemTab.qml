/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ

QQ.Rectangle {
    property bool selected: false;

    width:  parent.width
    x: selected ? parent.x - parent.anchors.leftMargin : parent.x + parent.width
    color: "#00000000"

    anchors.top: parent.top
    anchors.bottom: parent.bottom

    StandardBorder {
        anchors.rightMargin: -7
    }

    QQ.Behavior on x {
        QQ.NumberAnimation {
            duration: 200;
            easing.type: QQ.Easing.OutQuad
        }
    }
}
