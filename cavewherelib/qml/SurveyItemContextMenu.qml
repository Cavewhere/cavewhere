/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The context menu of a survey-tree row: Delete…, and Declination on a trip.
//
// A click on the row's name opens it and the name cell renames it, so the menu
// is left holding the verbs no cell of the row carries — a destructive verb
// always costs a deliberate right-click, a long press, or Shift+F10 on the
// current row, and a trip's calibration is set here rather than on a cell of
// its own.
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

    //The calibrations Declination acts on: the selected rows' trips when this
    //row is one of them, this row's trip otherwise. Read when the menu opens,
    //since the selection can have moved since the last time it did.
    property list<TripCalibration> tripCalibrations: []

    //Only a trip carries a calibration, so only a trip's menu offers one.
    readonly property bool isTripRow: contextMenuId.row !== null
                                      && !contextMenuId.row.isNode

    function showMenu(x: real, y: real) {
        contextMenuId.asking = false;
        contextMenuId.clickPos = Qt.point(x, y);
        contextMenuId.tripCalibrations =
                contextMenuId.surveyTree.tripCalibrationsFor(contextMenuId.row.object);
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

    //A node has no declination, and a Menu's `visible` opens the menu rather
    //than hiding its entry, so the submenu joins a trip row's menu and leaves
    //a node's instead of drawing itself as a dead entry.
    QQ.Instantiator {
        active: contextMenuId.isTripRow

        delegate: DeclinationSubmenu {
            tripCalibrations: contextMenuId.tripCalibrations
        }

        onObjectAdded: (index, object) => contextMenuId.addMenu(object as QC.Menu)

        //The submenu is the entry after Delete…, and the Instantiator destroys
        //what it made, so the menu gives it up rather than destroying it too.
        onObjectRemoved: (index, object) => contextMenuId.takeMenu(contextMenuId.count - 1)
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
