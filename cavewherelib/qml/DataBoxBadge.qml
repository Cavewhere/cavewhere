/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The small pill a survey-table cell shows when it carries a state its own value
// can't convey — a distance excluded from the plot, a station anchored by a fix.
// Anchors itself to the bottom of its host cell, so a cell just declares one of
// these with a text and nothing else.
QQ.Rectangle {
    id: rootId

    property string text: ""

    anchors.bottom: parent.bottom
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottomMargin: 2

    width: labelId.width + 4
    height: labelId.height - 2

    color: Theme.border

    QC.Label {
        id: labelId

        anchors.centerIn: parent
        anchors.verticalCenterOffset: 1

        font.pixelSize: Theme.fontSizeBody
        font.bold: true
        color: Theme.surfaceMuted

        // Every cell in the table carries a badge, almost all of them hidden, and
        // hiding an item doesn't stop it laying out its text. Blanking it takes
        // the empty-string fast path instead of building a QTextLayout per cell.
        text: rootId.visible ? rootId.text : ""
    }
}
