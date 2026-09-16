/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The context menu of a survey-tree row: Open, Rename… (native nodes only)
// and Delete….
//
// Delete lives here alone — no cell of the row carries a remove button — so
// a row's destructive verb always costs a deliberate right-click, a long
// press, or Shift+F10 on the current row.
//
// A row is eight cells, and a menu per cell would be eight menus saying the
// same thing, so the row holds one of these (in its Name cell) and every cell
// pops it. The row carries out Open and Rename; Delete goes to the view,
// which owns the prompt and knows how a node and a trip are each removed.
QC.Menu {
    id: contextMenuId
    objectName: "surveyItemContextMenu"

    //The SurveyTreeRow this menu belongs to. Typed as an Item because naming
    //SurveyTreeRow here would tie the two files into a cycle.
    property QQ.Item row

    //The SurveyTreeView around the row: it owns the Remove prompt and takes
    //the focus back when the menu closes.
    property QQ.Item surveyTree

    //Where the menu was asked for, in this menu's parent coordinates. The
    //prompt opens over the same spot.
    property point clickPos

    //True while Delete… is handing the row over to the Remove prompt, which
    //has the keyboard from then on.
    property bool asking: false

    function showMenu(x: real, y: real) {
        contextMenuId.asking = false;
        contextMenuId.clickPos = Qt.point(x, y);
        contextMenuId.popup(x, y);
    }

    QC.MenuItem {
        objectName: "surveyItemOpenMenuItem"
        text: qsTr("Open")

        onTriggered: contextMenuId.row.open()
    }

    ConditionalMenuItem {
        menu: contextMenuId
        insertIndex: 1
        active: contextMenuId.row !== null && contextMenuId.row.canRename
        itemObjectName: "surveyItemRenameMenuItem"
        text: qsTr("Rename…")

        onTriggered: contextMenuId.row.startRename()
    }

    QC.MenuSeparator {}

    QC.MenuItem {
        objectName: "surveyItemDeleteMenuItem"
        text: qsTr("Delete…")

        onTriggered: {
            contextMenuId.asking = true;
            contextMenuId.surveyTree.askRemove(contextMenuId.row,
                                               contextMenuId.clickPos.x,
                                               contextMenuId.clickPos.y);
        }
    }

    //The tree takes the keyboard back, except when the Remove prompt is the
    //thing now on screen: the tree's Enter opens a page, which would leave a
    //stranded prompt behind.
    onClosed: {
        if(!contextMenuId.asking) {
            contextMenuId.surveyTree.focusTree();
        }
    }
}
