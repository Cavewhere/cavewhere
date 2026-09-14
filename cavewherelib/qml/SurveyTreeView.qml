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

// The survey tree on the Data page: every cave, the nodes under it, and each
// node's trips as leaves, in one table of seven columns.
//
// The rows come from cwSurveyTreeModel through cwSurveyTreeFilterModel, and
// TreeView flattens the tree into view rows itself. Expansion is view state
// rather than model state, so everything this view knows about which rows are
// open lives here: the helpers below translate between a view row
// (expand/collapse/isExpanded) and a model index through
// rowAtIndex/index and the proxy's mapToSource/mapFromSource.
ColumnLayout {
    id: surveyTreeId

    //The prompt a row's Remove goes through. The rows reach it through the
    //view, since a TreeView delegate is handed nothing but its model roles.
    property RemoveAskBox removeAskBox

    //The cwSurveyNode or cwTrip the current row stands for, null when no row
    //is current.
    readonly property QQ.QtObject currentObject: surveyTreeId.objectAt(selectionModelId.currentIndex)

    //What the Remove prompt is about to remove. The prompt carries a name and
    //a message, so the row it stands for is held here until it answers.
    property QQ.QtObject pendingRemoveObject: null

    //What was open before the filter took over the rows. Kept by node id
    //rather than by row, since filtering rebuilds every row.
    property list<string> expansionBeforeFilter: []

    spacing: 0

    //The cwSurveyNode or cwTrip a proxy index stands for, null for a row that
    //is gone. The model answers for source indexes, so every lookup goes
    //through the filter first.
    function objectAt(proxyIndex) : QQ.QtObject {
        return sourceModelId.objectFor(filterModelId.mapToSource(proxyIndex));
    }

    //The cwSurveyNode or cwTrip at a view row, null when the row is gone.
    function objectAtRow(row: int) : QQ.QtObject {
        return surveyTreeId.objectAt(treeViewId.index(row, SurveyTreeModel.Name));
    }

    //Makes the first top-level row current as soon as the tree has rows, the
    //way the cave list this replaced made its first cave current: the page's
    //export verbs read the current row.
    function makeFirstRowCurrent() {
        if(selectionModelId.currentIndex.valid || filterModelId.rowCount() === 0) {
            return;
        }

        selectionModelId.setCurrentIndex(filterModelId.index(0, SurveyTreeModel.Name),
                                         ItemSelectionModel.NoUpdate);
    }

    //Opens every ancestor of \a object so its row is one of the view's rows.
    function expandTo(object: QQ.QtObject) {
        const sourceIndex = sourceModelId.indexOf(object);
        treeViewId.expandToIndex(filterModelId.mapFromSource(sourceIndex));
        treeViewId.forceLayout();
    }

    //Opens the page a row stands for: a node's cave page, a trip's trip page.
    //Both go through the page address, which walks and registers the parent
    //pages on the way, so a trip opens before its cave page ever existed.
    function openObject(object: QQ.QtObject) {
        if(object === null) {
            return;
        }

        const node = object as SurveyNode;
        const link = node !== null
                   ? linkGeneratorId.caveLink(node)
                   : linkGeneratorId.tripLink(object as Trip);
        if(link !== "") {
            RootData.pageSelectionModel.currentPageAddress = link;
        }
    }

    //Asks before removing what the SurveyTreeRow \a row stands for. The row
    //already carries its object, its name and the trips under it, so the
    //prompt is filled in from the row itself. It opens over the spot \a x,
    //\a y names in the row's coordinates.
    function askRemove(row: QQ.Item, x: real, y: real) {
        if(surveyTreeId.removeAskBox === null || row === null) {
            return;
        }

        const position = row.mapToItem(surveyTreeId.removeAskBox.parent, x, y);
        surveyTreeId.removeAskBox.x = position.x;
        surveyTreeId.removeAskBox.y = position.y;
        surveyTreeId.removeAskBox.removeName = row.name;
        surveyTreeId.removeAskBox.message =
                surveyTreeId.removeMessage(row.name, row.isNode, row.tripCount);
        surveyTreeId.pendingRemoveObject = row.object;
        surveyTreeId.removeAskBox.show();
    }

    //What the prompt says. A node takes its trips with it, so the question
    //names them.
    function removeMessage(name: string, isNode: bool, tripCount: int) : string {
        if(!isNode || tripCount === 0) {
            return qsTr("Remove <b>%1</b>?").arg(name);
        }
        if(tripCount === 1) {
            return qsTr("Remove <b>%1</b> and its 1 trip?").arg(name);
        }
        return qsTr("Remove <b>%1</b> and its %2 trips?").arg(name).arg(tripCount);
    }

    //Removes whatever the prompt asked about. A top-level node is a row of the
    //region, which is the undoable way to remove a cave; anything deeper is a
    //row of its own parent.
    function removePending() {
        const object = surveyTreeId.pendingRemoveObject;
        surveyTreeId.pendingRemoveObject = null;
        if(object === null) {
            return;
        }

        const node = object as SurveyNode;
        if(node !== null) {
            const cave = object as Cave;
            const regionRow = cave !== null ? RootData.region.indexOf(cave) : -1;
            if(regionRow >= 0) {
                RootData.region.removeCave(regionRow);
                return;
            }

            const parentNode = node.parentNode;
            if(parentNode !== null) {
                parentNode.removeNode(parentNode.indexOfNode(node));
            }
            return;
        }

        const trip = object as Trip;
        if(trip !== null && trip.parentNode !== null) {
            trip.parentNode.removeTrip(trip.parentNode.indexOf(trip));
        }
    }

    //Takes the keyboard back after a menu closes, so the arrow keys keep
    //moving the tree.
    function focusTree() {
        treeViewId.forceActiveFocus();
    }

    //Opens every row of the tree, and every row those rows bring with them.
    function expandAll() {
        treeViewId.expandRecursively();
        treeViewId.forceLayout();
    }

    //Closes every row of the tree.
    function collapseAll() {
        treeViewId.collapseRecursively();
        treeViewId.forceLayout();
    }

    //The ids of the nodes that are open right now. A filter change rebuilds
    //the view's rows, so the set is kept by node identity rather than by row.
    function expansionSnapshot() : list<string> {
        let ids = [];
        for(let row = 0; row < treeViewId.rows; row++) {
            if(treeViewId.isExpanded(row)) {
                const object = surveyTreeId.objectAtRow(row);
                if(object !== null) {
                    ids.push(String(object.id));
                }
            }
        }
        return ids;
    }

    //Reopens the nodes of a snapshot. Expanding a row adds the rows under it,
    //so walking forward reaches a restored node's own children.
    function restoreExpansion(ids: list<string>) {
        for(let row = 0; row < treeViewId.rows; row++) {
            const object = surveyTreeId.objectAtRow(row);
            if(object !== null && ids.indexOf(String(object.id)) >= 0) {
                treeViewId.expand(row);
                treeViewId.forceLayout();
            }
        }
    }

    //Shows the rows matching \a text and hides the rest. The proxy keeps a
    //match's ancestors, so opening every row while a filter is on is what
    //makes each match a row of the view; clearing puts back what was open
    //before, which the view snapshots on the way in.
    function applyFilter(text: string) {
        const wasFiltering = filterModelId.filterText.length > 0;
        const isFiltering = text.length > 0;

        if(!wasFiltering && isFiltering) {
            surveyTreeId.expansionBeforeFilter = surveyTreeId.expansionSnapshot();
        }

        filterModelId.filterText = text;
        treeViewId.forceLayout();

        if(isFiltering) {
            surveyTreeId.expandAll();
        } else if(wasFiltering) {
            surveyTreeId.collapseAll();
            surveyTreeId.restoreExpansion(surveyTreeId.expansionBeforeFilter);
            surveyTreeId.expansionBeforeFilter = [];
        }
    }

    //True when the node at \a row has rows of its own to show.
    function rowHasChildren(row: int) : bool {
        const node = surveyTreeId.objectAtRow(row) as SurveyNode;
        return node !== null && (node.childNodeCount + node.tripCount) > 0;
    }

    //Moves the current row, which is what the arrow keys and the tree's own
    //verbs share.
    function setCurrentRow(row: int) {
        if(row < 0 || row >= treeViewId.rows) {
            return;
        }

        selectionModelId.setCurrentIndex(treeViewId.index(row, SurveyTreeModel.Name),
                                         ItemSelectionModel.NoUpdate);
        treeViewId.positionViewAtRow(row, QQ.TableView.Contain);
    }

    //The delegate drawing the current row's Name cell, null when that row is
    //outside the viewport.
    function currentRowItem() : QQ.Item {
        return treeViewId.itemAtCell(Qt.point(SurveyTreeModel.Name, treeViewId.currentRow));
    }

    //The keys TableView leaves to the tree: → ← Space, Enter, and the two ways
    //to ask for the context menu. ↑ ↓ Home End stay TableView's own.
    function handleKey(event) {
        const row = treeViewId.currentRow;
        if(row < 0) {
            return;
        }

        switch(event.key) {
        case Qt.Key_Right:
            if(surveyTreeId.rowHasChildren(row) && !treeViewId.isExpanded(row)) {
                treeViewId.expand(row);
            } else {
                surveyTreeId.setCurrentRow(row + 1);
            }
            event.accepted = true;
            break;
        case Qt.Key_Left: {
            if(treeViewId.isExpanded(row)) {
                treeViewId.collapse(row);
            } else {
                const parentIndex = treeViewId.index(row, SurveyTreeModel.Name).parent;
                surveyTreeId.setCurrentRow(treeViewId.rowAtIndex(parentIndex));
            }
            event.accepted = true;
            break;
        }
        case Qt.Key_Space:
            treeViewId.toggleExpanded(row);
            event.accepted = true;
            break;
        case Qt.Key_Return:
        case Qt.Key_Enter:
            surveyTreeId.openObject(surveyTreeId.objectAtRow(row));
            event.accepted = true;
            break;
        case Qt.Key_Menu:
        case Qt.Key_F10: {
            if(event.key === Qt.Key_F10 && !(event.modifiers & Qt.ShiftModifier)) {
                return;
            }

            const rowItem = surveyTreeId.currentRowItem();
            if(rowItem !== null) {
                //Under the row's name, which is where the pointer would be.
                rowItem.showContextMenu(Theme.delegatePadding, rowItem.height);
            }
            event.accepted = true;
            break;
        }
        default:
            break;
        }
    }

    //A tree built over a region that already holds caves sees no insert.
    QQ.Component.onCompleted: surveyTreeId.makeFirstRowCurrent()

    //The prompt answers for whichever row asked for it.
    QQ.Connections {
        target: surveyTreeId.removeAskBox

        function onRemove() {
            surveyTreeId.removePending();
        }
    }

    RowLayout {
        id: toolBarId

        spacing: Theme.flowSpacing

        Layout.fillWidth: true
        Layout.bottomMargin: Theme.tightSpacing

        QC.TextField {
            id: filterFieldId
            objectName: "surveyTreeFilter"

            placeholderText: qsTr("Filter caves and trips")
            inputMethodHints: Qt.ImhNoPredictiveText
            font.pixelSize: Theme.fontSizeBody

            Layout.preferredWidth: Theme.treeFilterWidth

            onTextChanged: surveyTreeId.applyFilter(filterFieldId.text)
        }

        QQ.Item { Layout.fillWidth: true }

        QC.Button {
            objectName: "expandAllButton"
            text: qsTr("Expand all")

            onClicked: surveyTreeId.expandAll()
        }

        QC.Button {
            objectName: "collapseAllButton"
            text: qsTr("Collapse all")

            onClicked: surveyTreeId.collapseAll()
        }
    }

    QC.HorizontalHeaderView {
        id: headerId
        objectName: "surveyTreeHeader"

        syncView: treeViewId
        clip: true

        Layout.fillWidth: true

        delegate: QQ.Rectangle {
            id: headerCellId

            required property int column
            required property string display

            objectName: "surveyTreeHeaderCell" + headerCellId.column

            implicitHeight: headerLabelId.implicitHeight + 2 * Theme.delegatePadding
            color: Theme.surfaceMuted

            QC.Label {
                id: headerLabelId

                anchors.left: parent.left
                anchors.leftMargin: Theme.delegatePadding
                anchors.verticalCenter: parent.verticalCenter
                text: headerCellId.display
                color: Theme.textSubtle
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }

    //Unnamed on purpose: ObjectFinder matches a test's chain against every
    //named item between the window and the target, so a name here would sit
    //in the middle of the row chains the Data page's tests already use.
    QQ.TreeView {
        id: treeViewId

        //Handed to the rows, which see the view but not the page around it.
        //SurveyTreeRow reads both off its required `treeView`, so renaming
        //either one renames it there too. The tree is typed as an Item rather
        //than as SurveyTreeView: this is that file.
        property LinkGenerator linkGenerator: linkGeneratorId
        property QQ.Item surveyTree: surveyTreeId

        //What the Name column gives up to the fixed columns beside it.
        readonly property int fixedColumnsWidth: Theme.treeKindColumnWidth
                                                 + Theme.treeCountColumnWidth
                                                 + 2 * Theme.treeStatColumnWidth
                                                 + Theme.treeDateColumnWidth
                                                 + Theme.treeActionsColumnWidth

        clip: true
        keyNavigationEnabled: true
        pointerNavigationEnabled: true
        selectionBehavior: QQ.TableView.SelectRows
        //Enter opens the current row and renaming is the row's own editor, so
        //no key or tap starts a cell edit.
        editTriggers: QQ.TableView.NoEditTriggers

        Layout.fillWidth: true
        Layout.fillHeight: true

        model: SurveyTreeFilterModel {
            id: filterModelId

            sourceModel: SurveyTreeModel {
                id: sourceModelId
                region: RootData.region
            }
        }

        selectionModel: ItemSelectionModel {
            id: selectionModelId
            model: filterModelId
        }

        delegate: SurveyTreeRow {}

        //The fixed columns take what they need and the Name column takes the
        //rest, so a cave's name has the room a window gives it.
        columnWidthProvider: function(column) {
            switch(column) {
            case SurveyTreeModel.Kind:
                return Theme.treeKindColumnWidth;
            case SurveyTreeModel.Trips:
                return Theme.treeCountColumnWidth;
            case SurveyTreeModel.Length:
            case SurveyTreeModel.Depth:
                return Theme.treeStatColumnWidth;
            case SurveyTreeModel.LastSurvey:
                return Theme.treeDateColumnWidth;
            case SurveyTreeModel.Actions:
                return Theme.treeActionsColumnWidth;
            default:
                return Math.max(Theme.treeNameColumnMinimumWidth,
                                treeViewId.width - treeViewId.fixedColumnsWidth);
            }
        }

        onWidthChanged: treeViewId.forceLayout()

        //Handled before TableView sees the key, and accepted only for the keys
        //the tree owns, so ↑ ↓ Home End stay TableView's.
        QQ.Keys.onPressed: (event) => surveyTreeId.handleKey(event)

        LinkGenerator {
            id: linkGeneratorId
            pageSelectionModel: RootData.pageSelectionModel
        }

        //A node that owns a copied survey file opens one level as it arrives,
        //which is what a load of such a project does row by row.
        QQ.Connections {
            target: filterModelId

            function onRowsInserted(parent, first, last) {
                for(let row = first; row <= last; row++) {
                    const proxyIndex = filterModelId.index(row, SurveyTreeModel.Name, parent);
                    if(sourceModelId.isSourceRootIndex(filterModelId.mapToSource(proxyIndex))) {
                        //A row under a closed parent is no row of the view, so
                        //its ancestors open before the view is asked for it.
                        treeViewId.expandToIndex(proxyIndex);
                        treeViewId.forceLayout();

                        const viewRow = treeViewId.rowAtIndex(proxyIndex);
                        if(viewRow >= 0) {
                            treeViewId.expand(viewRow);
                        }
                    }
                }

                surveyTreeId.makeFirstRowCurrent();
            }

            function onModelReset() {
                surveyTreeId.makeFirstRowCurrent();
            }
        }
    }
}
