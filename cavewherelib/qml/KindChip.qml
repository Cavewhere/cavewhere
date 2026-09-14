/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The small label on a survey-tree row naming what the node is: "Cave",
// "Folder", "Compass project", … A sourced node reads in the accent color
// behind a lock, because a copied survey file owns its shape. Display only —
// changing a node's kind is the picker that lands with Folders.
QQ.Rectangle {
    id: chipId

    property alias text: labelId.text
    property bool sourced: false

    implicitWidth: contentRowId.implicitWidth + 2 * Theme.chipPadding
    implicitHeight: contentRowId.implicitHeight + 2 * Theme.tightSpacing
    radius: Theme.chipRadius
    color: chipId.sourced ? Theme.chipSourcedBackground : Theme.chipBackground
    visible: labelId.text !== ""

    QQ.Row {
        id: contentRowId

        anchors.centerIn: parent
        spacing: Theme.tightSpacing

        QC.Label {
            text: "🔒"
            font.pixelSize: Theme.fontSizeCaption
            visible: chipId.sourced
        }

        QC.Label {
            id: labelId

            font.pixelSize: Theme.fontSizeCaption
            color: chipId.sourced ? Theme.chipSourcedText : Theme.chipText
        }
    }
}
