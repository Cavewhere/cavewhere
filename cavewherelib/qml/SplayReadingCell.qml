/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// One read-only splay reading, drawn as a cell of the survey grid. The splay
// accent fill keeps a cluster reading as a group without becoming a band across
// the table.
QQ.Rectangle {
    property alias text: readingLabelId.text

    color: Theme.splaySurface
    border.color: Theme.borderSubtle
    border.width: 1

    QC.Label {
        id: readingLabelId
        anchors.centerIn: parent
        font.pixelSize: Theme.fontSizeSmall
    }
}
