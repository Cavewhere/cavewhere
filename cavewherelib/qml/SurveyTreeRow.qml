/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQml.Models
import cavewherelib

// One cell of the Data page's survey tree.
//
// TreeView is a TableView, so its delegate is instantiated once per (row,
// column) — Qt's documented model — and this file renders the cell its
// `column` names. A Loader picks the one component that column needs, so a
// cell carries the content of its own column and nothing else. The tree
// itself (indent and caret) lives in the Name column, which is the column
// TreeView marks with isTreeNode.
//
// The row's identity for tests rides on the cells that carry content:
// caveDelegate<row>/tripDelegate<row> on the Name cell, caveLength<row> and
// caveDepth<row> on the two stat cells. Per-cell delegates make the stats
// siblings of the name rather than its children, which is why the stat cells
// name themselves instead of being found under the row.
QQ.Item {
    id: rowId

    //Assigned by TreeView
    required property QQ.TreeView treeView
    required property bool isTreeNode
    required property bool expanded
    required property bool hasChildren
    required property int depth
    required property int row
    required property int column
    required property bool current
    required property bool selected

    //The row's model roles
    required property QQ.QtObject object
    required property int rowType
    required property string name
    required property string kindLabel
    required property bool isSourced
    required property int tripCount
    required property UnitValue length
    required property UnitValue depthValue
    required property date lastSurvey
    required property bool muted

    //A pooled delegate keeps the row it last drew, so it gives up its name
    //until TreeView hands it a new row.
    property bool pooled: false

    //True while this row's name is being edited. Only the Name cell reads it;
    //a rename asked for from another cell is routed to the Name cell first.
    property bool renaming: false

    readonly property bool isNode: rowId.rowType === SurveyTreeModel.Node
    readonly property SurveyNode node: rowId.object as SurveyNode

    //A sourced node's name belongs to its file, and a trip is renamed on its
    //own page, so only a native node offers Rename.
    readonly property bool canRename: rowId.isNode && !rowId.isSourced

    objectName: {
        if(rowId.pooled) {
            return "";
        }

        //Every named cell of a row carries the row's own kind, so
        //caveLength<row> names a node row's stat and a trip's stat names
        //itself.
        const prefix = rowId.isNode ? "cave" : "trip";

        switch(rowId.column) {
        case SurveyTreeModel.Name:
            return prefix + "Delegate" + rowId.row;
        case SurveyTreeModel.Kind:
            return prefix + "Kind" + rowId.row;
        case SurveyTreeModel.Length:
            return prefix + "Length" + rowId.row;
        case SurveyTreeModel.Depth:
            return prefix + "Depth" + rowId.row;
        default:
            return "";
        }
    }

    implicitWidth: contentLoaderId.implicitWidth + 2 * Theme.delegatePadding
    implicitHeight: Theme.treeRowHeight

    //The page the row stands for: a node's cave page, a trip's trip page.
    //The view opens it, so a row opened by a click, by the menu and by Enter
    //all take the same path.
    function open() {
        rowId.treeView.surveyTree.openObject(rowId.object);
    }

    //Adds a native trip to this node and opens it, as the cave page's Add Trip
    //does. The new trip is the node's last one.
    function addTrip() {
        if(rowId.node === null) {
            return;
        }

        rowId.node.addTrip();
        const trip = rowId.node.trip(rowId.node.tripCount - 1);
        rowId.treeView.surveyTree.expandTo(trip);
        rowId.treeView.surveyTree.openObject(trip);
    }

    //The cell of this row that carries the tree's caret, the name, the name
    //editor and the row's one context menu. Every other cell hands those to it.
    function nameCell() : QQ.Item {
        return rowId.column === SurveyTreeModel.Name
             ? rowId
             : rowId.treeView.itemAtCell(Qt.point(SurveyTreeModel.Name, rowId.row));
    }

    //Opens the name editor, wherever on the row the rename was asked for.
    function startRename() {
        const cell = rowId.nameCell();
        if(cell !== null) {
            cell.renaming = true;
        }
    }

    //Shows the row's context menu at \a x, \a y in this cell's coordinates.
    function showContextMenu(x: real, y: real) {
        const cell = rowId.nameCell();
        if(cell === null) {
            return;
        }

        const position = rowId.mapToItem(cell, x, y);
        cell.popupContextMenu(position.x, position.y);
    }

    //Pops this cell's own menu. Only the Name cell carries one.
    function popupContextMenu(x: real, y: real) {
        const menu = contextMenuLoaderId.item as SurveyItemContextMenu;
        if(menu !== null) {
            menu.showMenu(x, y);
        }
    }

    function toggleExpanded() {
        const index = rowId.treeView.index(rowId.row, rowId.column);
        rowId.treeView.selectionModel.setCurrentIndex(index, ItemSelectionModel.NoUpdate);
        rowId.treeView.toggleExpanded(rowId.row);
    }

    QQ.TableView.onPooled: {
        rowId.pooled = true;
        rowId.renaming = false;
    }
    QQ.TableView.onReused: rowId.pooled = false

    TableRowBackground {
        anchors.fill: parent
        //A tap or an arrow key moves the current index, which lands on one
        //cell — the whole row reads as selected by following its row.
        isSelected: rowId.selected || rowId.row === rowId.treeView.currentRow
        rowIndex: rowId.row
    }

    QQ.Loader {
        id: contentLoaderId

        anchors.fill: parent
        anchors.leftMargin: Theme.delegatePadding
        anchors.rightMargin: Theme.delegatePadding

        sourceComponent: {
            switch(rowId.column) {
            case SurveyTreeModel.Name:
                return nameCellComponent;
            case SurveyTreeModel.Kind:
                return kindCellComponent;
            case SurveyTreeModel.Trips:
                return tripCountCellComponent;
            case SurveyTreeModel.Length:
            case SurveyTreeModel.Depth:
                return statCellComponent;
            case SurveyTreeModel.LastSurvey:
                return lastSurveyCellComponent;
            case SurveyTreeModel.Actions:
                return actionsCellComponent;
            default:
                return null;
            }
        }
    }

    //A right-click or a long press anywhere on the row asks for the row's
    //menu, wherever on the row the pointer is.
    QQ.TapHandler {
        acceptedButtons: Qt.RightButton
        acceptedDevices: QQ.PointerDevice.Mouse | QQ.PointerDevice.TouchPad

        onTapped: (eventPoint) => rowId.showContextMenu(eventPoint.position.x,
                                                        eventPoint.position.y)
    }

    // Touch-only so mouse left-clicks pass through to the name link below.
    QQ.TapHandler {
        acceptedDevices: QQ.PointerDevice.TouchScreen

        onLongPressed: rowId.showContextMenu(point.position.x, point.position.y)
    }

    //The row's one menu, carried by the cell the tree's name lives in.
    QQ.Loader {
        id: contextMenuLoaderId

        active: rowId.column === SurveyTreeModel.Name
        sourceComponent: contextMenuComponent
    }

    //The Name cell of both a node and a trip: a node carries the caret and the
    //badges, a trip keeps the caret's slot so its name lines up under its
    //node's name.
    QQ.Component {
        id: nameCellComponent

        QQ.Item {
            RowLayout {
                anchors.fill: parent
                spacing: Theme.tightSpacing

                QQ.Item {
                    Layout.preferredWidth: rowId.depth * Theme.treeIndent
                    Layout.preferredHeight: 1
                }

                QQ.Item {
                    Layout.preferredWidth: Theme.treeCaretWidth
                    Layout.fillHeight: true

                    QC.Label {
                        anchors.centerIn: parent
                        visible: rowId.hasChildren
                        text: rowId.expanded ? "▾" : "▸"
                        color: Theme.textSubtle
                    }

                    QQ.TapHandler {
                        enabled: rowId.hasChildren
                        onSingleTapped: rowId.toggleExpanded()
                    }
                }

                //A row whose node has left the region draws one more frame,
                //so the badges answer for a node that is already gone.
                ErrorIconBar {
                    visible: rowId.isNode
                    errorModel: rowId.node !== null ? rowId.node.errorModel : null
                }

                ExternalSolveBadge {
                    owner: rowId.node
                    externallyBacked: rowId.node !== null
                                      && !rowId.node.externalCenterline.isEmpty
                }

                LinkText {
                    objectName: rowId.isNode ? "caveLink" : "tripLink"
                    text: rowId.name
                    color: rowId.isNode ? Theme.textLink : Theme.textSubtle
                    elide: QQ.Text.ElideRight
                    visible: !rowId.renaming

                    Layout.fillWidth: true

                    onClicked: rowId.open()
                }

                //The name is a link first, and CoreClickTextInput opens its
                //editor on its own tap, so the field exists only while the
                //row is being renamed and hands the name back on commit.
                QQ.Loader {
                    id: renameLoaderId

                    active: rowId.renaming
                    sourceComponent: renameFieldComponent

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
    }

    QQ.Component {
        id: kindCellComponent

        //A Loader resizes what it loads, so the chip sits in a filling Item
        //and keeps the pill size its own implicit size asks for.
        QQ.Item {
            KindChip {
                objectName: "kindChip"

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: rowId.kindLabel
                sourced: rowId.isSourced
            }
        }
    }

    QQ.Component {
        id: tripCountCellComponent

        QC.Label {
            verticalAlignment: QQ.Text.AlignVCenter
            text: rowId.isNode ? rowId.tripCount : ""
            color: rowId.muted ? Theme.textSubtle : Theme.text
        }
    }

    QQ.Component {
        id: statCellComponent

        SelectableCaveStat {
            readonly property bool isDepthCell: rowId.column === SurveyTreeModel.Depth

            anchors.verticalCenter: parent.verticalCenter
            unitValue: isDepthCell ? rowId.depthValue : rowId.length
            depth: isDepthCell
        }
    }

    QQ.Component {
        id: lastSurveyCellComponent

        QC.Label {
            verticalAlignment: QQ.Text.AlignVCenter
            text: isNaN(rowId.lastSurvey.getTime())
                  ? ""
                  : Qt.formatDate(rowId.lastSurvey, Qt.ISODate)
            color: rowId.muted ? Theme.textSubtle : Theme.text
        }
    }

    QQ.Component {
        id: renameFieldComponent

        CoreClickTextInput {
            id: renameFieldId

            text: rowId.name

            //The field is laid out by the row before it can be edited, so the
            //editor it opens over itself lands on the name it replaced.
            QQ.Component.onCompleted: Qt.callLater(() => renameFieldId.openEditor())

            //The editor closes on a commit and on a press somewhere else; both
            //give the row's name back to the link. The close comes first and
            //the committed name second, so the field gives up its place on the
            //next turn of the loop rather than during the commit that is still
            //running through it.
            onIsEdittingChanged: {
                if(!renameFieldId.isEditting) {
                    Qt.callLater(() => rowId.renaming = false);
                }
            }

            //setName refuses a name a sibling already carries, which is the
            //same rule the cave page's title commits through.
            onFinishedEditting: (newText) => {
                if(rowId.object !== null) {
                    rowId.object.name = newText;
                }
            }
        }
    }

    QQ.Component {
        id: contextMenuComponent

        SurveyItemContextMenu {
            parent: rowId
            row: rowId
            surveyTree: rowId.treeView.surveyTree
        }
    }

    QQ.Component {
        id: actionsCellComponent

        QQ.Item {
            ContextMenuButton {
                objectName: "rowActionsButton"

                anchors.centerIn: parent
                iconSource: "qrc:/twbs-icons/icons/three-dots.svg"
                menu: rowActionsMenuComponent
            }
        }
    }

    //The row's ⋯ menu: the verbs that add and open, never the one that
    //removes — Delete… belongs to the right-click menu alone.
    QQ.Component {
        id: rowActionsMenuComponent

        QC.Menu {
            id: rowActionsMenuId
            objectName: "rowActionsMenu"

            QC.MenuItem {
                objectName: "rowOpenMenuItem"
                text: qsTr("Open")

                onTriggered: rowId.open()
            }

            //Add Trip names the slot it goes in, since a pooled row that
            //becomes a node row activates these two in binding order rather
            //than in the order the menu reads them.
            ConditionalMenuItem {
                menu: rowActionsMenuId
                insertIndex: 1
                active: rowId.isNode
                itemObjectName: "rowAddTripMenuItem"
                text: qsTr("Add Trip")

                onTriggered: rowId.addTrip()
            }

            ConditionalMenuItem {
                menu: rowActionsMenuId
                active: rowId.canRename
                itemObjectName: "rowRenameMenuItem"
                text: qsTr("Rename…")

                onTriggered: rowId.startRename()
            }

            onClosed: rowId.treeView.surveyTree.focusTree()
        }
    }
}
