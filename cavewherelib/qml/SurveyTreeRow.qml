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

    readonly property bool isNode: rowId.rowType === SurveyTreeModel.Node
    readonly property SurveyNode node: rowId.object as SurveyNode

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
    //Both go through the page address so the parent pages are walked and
    //registered on the way.
    function open() {
        const link = rowId.isNode
                   ? rowId.treeView.linkGenerator.caveLink(rowId.object)
                   : rowId.treeView.linkGenerator.tripLink(rowId.object);
        if(link !== "") {
            RootData.pageSelectionModel.currentPageAddress = link;
        }
    }

    function toggleExpanded() {
        const index = rowId.treeView.index(rowId.row, rowId.column);
        rowId.treeView.selectionModel.setCurrentIndex(index, ItemSelectionModel.NoUpdate);
        rowId.treeView.toggleExpanded(rowId.row);
    }

    QQ.TableView.onPooled: rowId.pooled = true
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
            default:
                //The Actions column holds the row's ⋯ menu, which lands with
                //the context menus.
                return null;
            }
        }
    }

    //Remove lives here until the survey-tree context menu replaces it. Every
    //cell carries it, so a right-click anywhere on the row offers it; only a
    //top-level node has a region row to remove.
    DataRightClickMouseMenu {
        anchors.fill: parent
        enabled: rowId.isNode && rowId.depth === 0
        removeChallenge: rowId.treeView.removeAskBox
        name: rowId.name
        row: rowId.node !== null ? RootData.region.indexOf(rowId.node) : -1
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

                    Layout.fillWidth: true

                    onClicked: rowId.open()
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
}
