/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ

QQ.Rectangle {
    property alias text: textArea.text

    width: Math.floor(textArea.width) + 20
    height: 40

    border.color: "lightgray"
    border.width: 1;

    Text {
        id: textArea
        anchors.centerIn: parent
    }
}
