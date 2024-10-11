/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ

QQ.Rectangle {
    property alias text: textArea.text

    width: textArea.width + 20
    height: 40

    border.color: "lightgray"
    border.width: 1;

    Text {
        id: textArea
        anchors.centerIn: parent
    }
}
