/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The context menu of a survey-tree row: Add on a node, Move to…, Reload and
// Replace… on a source root, Delete…, and Declination on a trip. A copied
// file's data goes all or nothing, so Delete… sits on native rows, the source
// root and an attached trip, which leaves with its own file.
//
// A click on the row's name opens it and the name cell renames it, so the menu
// is left holding the verbs no cell of the row carries — the Add verbs for the
// node's position, a destructive verb that always costs a deliberate
// right-click, a long press, or Shift+F10 on the current row, and a trip's
// calibration, set here rather than on a cell of its own.
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

    //The menu's entries in the order they sit, by objectName. The entries
    //join and leave as the row changes, so each one takes its place by rank
    //rather than beside a neighbor that may be absent.
    readonly property list<string> entryOrder: [
        "addVerbsSubmenu",
        "surveyItemMoveMenuItem",
        "surveyItemReloadMenuItem",
        "surveyItemReplaceMenuItem",
        "surveyItemDeleteMenuItem",
        "declinationSubmenu"
    ]

    //The calibrations Declination acts on: the selected rows' trips when this
    //row is one of them, this row's trip otherwise. Read when the menu opens,
    //since the selection can have moved since the last time it did.
    property list<TripCalibration> tripCalibrations: []

    //Whether the row may be moved: survey data an attached file places stays
    //where its file puts it, so its menu offers no Move to…. Read when the
    //menu opens, like the calibrations.
    property bool canMove: false

    //Only a trip carries a calibration, so only a trip's menu offers one.
    readonly property bool isTripRow: contextMenuId.row !== null
                                      && !contextMenuId.row.isNode

    //Only a node holds anything, so only a node's menu offers Add.
    readonly property bool isNodeRow: contextMenuId.row !== null
                                      && contextMenuId.row.isNode

    //Reload and Replace act on a copied file's whole subtree, so only its
    //root's menu offers them.
    readonly property bool isSourceRootRow: contextMenuId.isNodeRow
                                            && contextMenuId.row.node !== null
                                            && contextMenuId.row.node.isSourceRoot

    //Whether the row may be deleted on its own: a node or trip a copied file
    //derives goes only with its whole file, which is the source root's
    //Delete….
    readonly property bool canDelete: contextMenuId.isNodeRow
                                      ? contextMenuId.row.node !== null
                                        && (!contextMenuId.row.node.isSourced
                                            || contextMenuId.row.node.isSourceRoot)
                                      : contextMenuId.row !== null
                                        && contextMenuId.row.trip !== null
                                        && contextMenuId.row.trip.sourcePath === ""

    //Whether Reload can find the file the root was copied from. That follows
    //the disk and has no change signal, so it is read when the menu opens.
    property bool canReload: false

    //Where an entry named \a name goes: ahead of the first entry that ranks
    //after it, or at the end.
    function insertPosition(name: string) : int {
        const rank = contextMenuId.entryOrder.indexOf(name);
        for(let i = 0; i < contextMenuId.count; i++) {
            const submenu = contextMenuId.menuAt(i);
            const entryName = submenu !== null ? submenu.objectName : contextMenuId.itemAt(i).objectName;
            if(contextMenuId.entryOrder.indexOf(entryName) > rank) {
                return i;
            }
        }
        return contextMenuId.count;
    }

    function insertEntry(item: QC.MenuItem) {
        contextMenuId.insertItem(contextMenuId.insertPosition(item.objectName), item);
    }

    function insertSubmenu(menu: QC.Menu) {
        contextMenuId.insertMenu(contextMenuId.insertPosition(menu.objectName), menu);
    }

    //Hands the entry back to the Instantiator, which owns its destruction.
    function takeEntry(item: QC.MenuItem) {
        const position = contextMenuId.indexOfItem(item);
        if(position >= 0) {
            contextMenuId.takeItem(position);
        }
    }

    //The position of the submenu \a menu among this menu's entries, -1 when
    //it holds none.
    function indexOfMenu(menu: QC.Menu) : int {
        for(let i = 0; i < contextMenuId.count; i++) {
            if(contextMenuId.menuAt(i) === menu) {
                return i;
            }
        }
        return -1;
    }

    //The position of \a item among this menu's entries, -1 when it holds none.
    function indexOfItem(item: QC.MenuItem) : int {
        for(let i = 0; i < contextMenuId.count; i++) {
            if(contextMenuId.itemAt(i) === item) {
                return i;
            }
        }
        return -1;
    }

    //Hands the submenu back to the Instantiator, which owns its destruction.
    function takeSubmenu(menu: QC.Menu) {
        const position = contextMenuId.indexOfMenu(menu);
        if(position >= 0) {
            contextMenuId.takeMenu(position);
        }
    }

    function showMenu(x: real, y: real) {
        contextMenuId.asking = false;
        contextMenuId.clickPos = Qt.point(x, y);
        contextMenuId.tripCalibrations =
                contextMenuId.surveyTree.tripCalibrationsFor(contextMenuId.row.object);
        contextMenuId.canMove =
                RegionSurveyTree.isMovableIndex(RegionSurveyTree.indexOf(contextMenuId.row.object));
        contextMenuId.canReload = contextMenuId.isSourceRootRow
                && RootData.externalCenterlineManager.canReloadFromSource(contextMenuId.row.node)
                && !RootData.externalCenterlineManager.isOwnerBusy(contextMenuId.row.node.id);
        contextMenuId.popup(x, y);
        //An entry that joined at the end takes its first click only after the
        //list lays out again (likely because it lay past the list's old height).
        const list = contextMenuId.contentItem as QQ.ListView;
        if(list !== null) {
            list.forceLayout();
        }
    }

    //The node's Add verbs lead the menu. A Menu's `visible` opens a submenu
    //rather than hiding its entry, so the submenu joins a node row's menu and
    //leaves a trip's instead.
    QQ.Instantiator {
        active: contextMenuId.isNodeRow

        delegate: AddVerbsMenu {
            objectName: "addVerbsSubmenu"
            title: qsTr("Add")
            node: contextMenuId.row !== null ? contextMenuId.row.node : null

            onAdded: (object) => contextMenuId.surveyTree.revealAdded(object)
            onAttachSurveyFileRequested: (node) => contextMenuId.surveyTree.attachSurveyFile(node)
        }

        onObjectAdded: (index, object) => contextMenuId.insertSubmenu(object as QC.Menu)
        onObjectRemoved: (index, object) => contextMenuId.takeSubmenu(object as QC.Menu)
    }

    //Arms the move; the tree's own rows then pick where it goes. A hidden
    //MenuItem keeps its slot in the menu's list, so the entry joins a movable
    //row's menu and leaves every other one, the way the submenus do.
    QQ.Instantiator {
        active: contextMenuId.canMove

        delegate: QC.MenuItem {
            objectName: "surveyItemMoveMenuItem"
            text: qsTr("Move to…")

            onTriggered: contextMenuId.surveyTree.moveObject(contextMenuId.row.object)
        }

        onObjectAdded: (index, object) => contextMenuId.insertEntry(object as QC.MenuItem)
        onObjectRemoved: (index, object) => contextMenuId.takeEntry(object as QC.MenuItem)
    }

    //Reload and Replace… join a source root's menu, the way Move to… joins a
    //movable row's.
    QQ.Instantiator {
        model: contextMenuId.isSourceRootRow ? ["reload", "replace"] : []

        delegate: QC.MenuItem {
            id: sourceVerbItemId

            required property string modelData

            readonly property bool isReload: sourceVerbItemId.modelData === "reload"

            objectName: sourceVerbItemId.isReload ? "surveyItemReloadMenuItem" : "surveyItemReplaceMenuItem"
            text: sourceVerbItemId.isReload ? qsTr("Reload") : qsTr("Replace…")
            enabled: sourceVerbItemId.isReload ? contextMenuId.canReload : true

            onTriggered: {
                if(sourceVerbItemId.isReload) {
                    RootData.reloadSource(contextMenuId.row.node);
                } else {
                    contextMenuId.surveyTree.replaceSource(contextMenuId.row.node);
                }
            }
        }

        onObjectAdded: (index, object) => contextMenuId.insertEntry(object as QC.MenuItem)
        onObjectRemoved: (index, object) => contextMenuId.takeEntry(object as QC.MenuItem)
    }

    QQ.Instantiator {
        active: contextMenuId.canDelete

        delegate: QC.MenuItem {
            objectName: "surveyItemDeleteMenuItem"
            text: qsTr("Delete…")

            onTriggered: {
                contextMenuId.asking = true;
                contextMenuId.surveyTree.askRemove(contextMenuId.row,
                                                   contextMenuId.clickPos.x,
                                                   contextMenuId.clickPos.y);
            }
        }

        onObjectAdded: (index, object) => contextMenuId.insertEntry(object as QC.MenuItem)
        onObjectRemoved: (index, object) => contextMenuId.takeEntry(object as QC.MenuItem)
    }

    //A node has no declination, and a Menu's `visible` opens the menu rather
    //than hiding its entry, so the submenu joins a trip row's menu and leaves
    //a node's instead of drawing itself as a dead entry.
    QQ.Instantiator {
        active: contextMenuId.isTripRow

        delegate: DeclinationSubmenu {
            tripCalibrations: contextMenuId.tripCalibrations
        }

        onObjectAdded: (index, object) => contextMenuId.insertSubmenu(object as QC.Menu)

        onObjectRemoved: (index, object) => contextMenuId.takeSubmenu(object as QC.Menu)
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
