/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick.Controls as QC
import cavewherelib

// The menu one splay row pops, from its ⋯ or from a right click. Moving arms
// the splay and waits for a station to be clicked; removing happens at once,
// since the cluster it came out of is right there to see and a bad shot out of
// a download is cheap to lose.
ContextMenuArea {
    id: splayRowMenu
    objectName: "splayRowMenu"

    required property SurveyEditorModel model
    required property cwSurveyEditorRowIndex rowIndex

    QC.MenuItem {
        objectName: "moveSplayMenuItem"
        text: "Move to…"
        onTriggered: splayRowMenu.model.startSplayMove(splayRowMenu.rowIndex, false)
    }

    QC.MenuItem {
        objectName: "removeSplayMenuItem"
        text: "Remove splay"
        onTriggered: splayRowMenu.model.removeSplayAt(splayRowMenu.rowIndex)
    }
}
