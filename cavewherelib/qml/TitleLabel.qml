/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QQ.Rectangle {
    property alias text: textArea.text

    implicitWidth: Math.floor(textArea.width) + 10
    height: 40

    border.color: Theme.borderSubtle
    border.width: 1;

    color: Theme.surface

    QC.Label {
        id: textArea
        anchors.centerIn: parent
    }
}
