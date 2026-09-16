/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The context menu of a survey-tree row: Delete… alone.
//
// A click on the row's name opens it and the name cell renames it, so the menu
// is left holding the one verb no cell of the row carries — a destructive verb
// always costs a deliberate right-click, a long press, or Shift+F10 on the
// current row.
//
// A row is eight cells, and a menu per cell would be eight menus saying the
// same thing, so the row holds one of these (in its Name cell) and every cell
// pops it. Delete goes to the view, which owns the prompt and knows how a node
// and a trip are each removed.
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
