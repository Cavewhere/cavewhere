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
import "Utils.js" as Utils

// One cell of the Data page's survey tree.
//
// TreeView is a TableView, so its delegate is instantiated once per (row,
// column) — Qt's documented model — and this file renders the cell its
// `column` names. A Loader picks the one component that column needs, so a
// cell carries the content of its own column and nothing else. The tree
// itself (indent and caret) lives in the Name column, which is the column
// TreeView marks with isTreeNode.
//
// A node row's Length cell reads a UnitValue and a trip row's the plain number
// its length task added up, so `length` is typed loosely enough to hold both.
//
// The row's identity for tests rides on the cells that carry content:
// caveDelegate<row>/tripDelegate<row> on the Name cell and, with the same
// cave/trip prefix, Kind, Stations, Length, Depth, Date and Decl on the cells
// that carry those. Per-cell delegates make the stats siblings of the name
// rather than its children, which is why the stat cells name themselves
// instead of being found under the row.
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
    required property int stationCount
    required property var length
    required property UnitValue depthValue
    required property date dateValue
    required property real declination
    required property bool autoDeclination
    required property bool muted

    //A pooled delegate keeps the row it last drew, so it gives up its name
    //until TreeView hands it a new row.
    property bool pooled: false

    //True while this row's name is being edited. Only the Name cell reads it;
    //a rename asked for from another cell is routed to the Name cell first.
    property bool renaming: false

    readonly property bool isNode: rowId.rowType === SurveyTreeModel.Node
    readonly property SurveyNode node: rowId.object as SurveyNode
    readonly property Trip trip: rowId.object as Trip

    //A trip row sits under the cave it belongs to, so its cells are drawn
    //quieter than the cave's own.
    readonly property QQ.color textColor: rowId.muted ? Theme.textSubtle : Theme.text

    //True while the row's survey data comes from an attached file: a node owns
    //an external centerline, a trip is a window into one.
    readonly property bool externallyBacked: rowId.isNode
        ? (rowId.node !== null && !rowId.node.externalCenterline.isEmpty)
        : (rowId.trip !== null && rowId.trip.externallyBacked)

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
        case SurveyTreeModel.Stations:
            return prefix + "Stations" + rowId.row;
        case SurveyTreeModel.Date:
            return prefix + "Date" + rowId.row;
        case SurveyTreeModel.Decl:
            return prefix + "Decl" + rowId.row;
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

    //The name a cell inside this row answers to. A pooled delegate stays a
    //child of the view while it holds the row it last drew, so its cells give
    //up their names alongside the row's own: a chain or a count that looks for
    //them then finds the rows the view is showing and no others.
    function cellName(name: string) : string {
        return rowId.pooled ? "" : name;
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
            case SurveyTreeModel.Stations:
            case SurveyTreeModel.Date:
                return textCellComponent;
            case SurveyTreeModel.Length:
                return rowId.isNode ? statCellComponent : tripLengthCellComponent;
            case SurveyTreeModel.Depth:
                return statCellComponent;
            case SurveyTreeModel.Decl:
                //Only a trip carries a declination.
                return rowId.isNode ? null : declinationCellComponent;
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

                //A row whose object has left the region draws one more frame,
                //so the badges answer for a node or a trip that is already gone.
                ErrorIconBar {
                    errorModel: rowId.isNode
                        ? (rowId.node !== null ? rowId.node.errorModel : null)
                        : (rowId.trip !== null ? rowId.trip.errorModel : null)
                }

                ExternalSolveBadge {
                    owner: rowId.object
                    externallyBacked: rowId.externallyBacked
                    //A node has no account of its own of a missing station.
                    fallbackError: rowId.trip !== null
                                   ? rowId.trip.externalStationsError
                                   : ""
                }

                LinkText {
                    objectName: rowId.cellName(rowId.isNode ? "caveLink" : "tripLink")
                    //The paperclip marks a trip that windows an attached file;
                    //a node says so with its Kind chip instead.
                    text: (rowId.externallyBacked && !rowId.isNode ? "📎 " : "")
                          + rowId.name
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
                objectName: rowId.cellName("kindChip")

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: rowId.kindLabel
                sourced: rowId.isSourced
            }
        }
    }

    //A Loader stretches what it loads to the cell, and a stretched RowLayout
    //would strand the unit at the far edge of a wide column, so every cell
    //whose content is a row of labels sits in a filling Item and keeps the
    //width its own content asks for.
    QQ.Component {
        id: statCellComponent

        QQ.Item {
            SelectableCaveStat {
                readonly property bool isDepthCell: rowId.column === SurveyTreeModel.Depth

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                unitValue: isDepthCell ? rowId.depthValue : (rowId.length as UnitValue)
                depth: isDepthCell
            }
        }
    }

    //A trip's length is the number its length task added up, and it reads as
    //one cell with the unit beside it — the shape SelectableCaveStat gives a
    //node's solved length. The task reports in the unit the trip was surveyed
    //in, so the cell converts to the unit the project displays: a cave and the
    //trips under it then state the same kind of number in one column.
    QQ.Component {
        id: tripLengthCellComponent

        QQ.Item {
            id: tripLengthCellId

            //A pooled cell is handed its new row's roles one at a time and
            //before the Loader swaps in the component that row's kind asks
            //for, so a node's cwLength can reach these bindings for one turn.
            readonly property bool hasLength: rowId.trip !== null
                                              && typeof rowId.length === "number"
            readonly property real meters: tripLengthCellId.hasLength
                ? Units.convertLength(rowId.length, rowId.trip.calibration.distanceUnit, Units.Meters)
                : 0.0
            readonly property int displayUnit: Units.lengthDisplayUnit(tripLengthCellId.meters,
                                                                      ProjectUnits.unitSystem)

            RowLayout {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.delegatePadding

                SelectableValue {
                    objectName: rowId.cellName("value")
                    text: tripLengthCellId.hasLength
                          ? Utils.fixed(Units.convertLength(tripLengthCellId.meters,
                                                            Units.Meters,
                                                            tripLengthCellId.displayUnit), 2)
                          : ""
                    color: rowId.textColor
                    font.pixelSize: Theme.fontSizeUI
                }

                QC.Label {
                    objectName: rowId.cellName("unit")
                    text: tripLengthCellId.hasLength
                          ? Units.lengthUnitName(tripLengthCellId.displayUnit)
                          : ""
                    color: rowId.textColor
                }
            }
        }
    }

    //The cells that are a line of text and nothing else. A trip belongs to one
    //day and a node to as many as it holds trips, so only a trip row dates
    //itself; only a node row counts trips, and both count stations.
    QQ.Component {
        id: textCellComponent

        QC.Label {
            objectName: rowId.cellName("value")

            verticalAlignment: QQ.Text.AlignVCenter
            color: rowId.textColor
            text: {
                switch(rowId.column) {
                case SurveyTreeModel.Trips:
                    return rowId.isNode ? rowId.tripCount : "";
                case SurveyTreeModel.Stations:
                    return rowId.stationCount;
                case SurveyTreeModel.Date:
                    return isNaN(rowId.dateValue.getTime())
                         ? ""
                         : Qt.formatDate(rowId.dateValue, Qt.ISODate);
                default:
                    return "";
                }
            }
        }
    }

    //Only a trip carries a declination, and it reads the way the cave page's
    //trip table reads it: the angle, then whether it was picked automatically.
    //An externally backed trip takes its declination from its file, where each
    //block may set its own, so the row has no one angle to show.
    QQ.Component {
        id: declinationCellComponent

        QQ.Item {
            //A wide angle is held to the column's width so it elides instead
            //of drawing past the cell, the way the cave page holds its own.
            clip: true

            RowLayout {
                id: declRowId

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.tightSpacing

                QC.Label {
                    objectName: rowId.cellName("value")
                    visible: !rowId.externallyBacked
                    text: Utils.fixed(rowId.declination, 2) + "°"
                    color: rowId.textColor
                }

                QC.Label {
                    objectName: rowId.cellName("declinationMode")
                    visible: !rowId.externallyBacked
                    text: rowId.autoDeclination ? qsTr("auto") : qsTr("manual")
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeCaption
                }

                QC.Label {
                    objectName: rowId.cellName("declinationEmDash")
                    visible: rowId.externallyBacked
                    text: "—"
                    color: Theme.textSubtle
                }
            }
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
}
