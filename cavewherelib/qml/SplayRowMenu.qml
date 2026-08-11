/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick.Controls as QC
import cavewherelib

// The menu one splay row pops, from its ⋯ or from a right click. Removing a
// single splay happens at once: the cluster it came out of is right there to
// see, and a bad shot out of a download is cheap to lose.
ContextMenuArea {
    id: splayRowMenu
    objectName: "splayRowMenu"

    required property SurveyEditorModel model
    required property cwSurveyEditorRowIndex rowIndex

    QC.MenuItem {
        objectName: "removeSplayMenuItem"
        text: "Remove splay"
        onTriggered: splayRowMenu.model.removeSplayAt(splayRowMenu.rowIndex)
    }
}
