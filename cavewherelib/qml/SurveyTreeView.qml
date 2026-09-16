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

// The survey tree: every cave, the nodes under it, and each node's trips as
// leaves, in one table of eight columns. The Data page shows the whole region
// and the cave page shows the same tree rooted at one cave — one view, one set
// of rows, told apart by rootNode alone.
//
// The rows come from cwSurveyTreeModel through cwSurveyTreeFilterModel, and
// TreeView flattens the tree into view rows itself. Expansion is view state
// rather than model state, so everything this view knows about which rows are
// open lives here: the helpers below translate between a view row
// (expand/collapse/isExpanded) and a model index through
// rowAtIndex/index and the proxy's mapToSource/mapFromSource.
//
// The rows stand in the order each node lists its children and trips in until
// a header cell is clicked; the header is the one sort control, and the arrow
// it carries says which column the rows are ordered by.
ColumnLayout {
    id: surveyTreeId

    //The prompt a row's Remove goes through. The rows reach it through the
    //view, since a TreeView delegate is handed nothing but its model roles.
    property RemoveAskBox removeAskBox

    //The node whose children the tree shows: null shows the whole region, a
    //cave shows that cave's own rows. TreeView draws the CHILDREN of its
    //rootIndex, so a cave here leaves the cave itself off and its trips as
    //the view's top-level rows — which is what the cave page shows.
    property SurveyNode rootNode: null

    //Whether the view scrolls itself. A page that scrolls as a whole gives
    //the tree its full height instead and keeps one scrollbar.
    property bool scrollable: true

    //The height every row of the tree needs, the toolbar and header included.
    //Counted from the rows rather than read off the view's contentHeight: a
    //view sized by what it has laid out, and laying out by the height it was
    //given, keeps whichever height it started with. Every row is
    //Theme.treeRowHeight tall.
    readonly property real fullHeight: toolBarId.height
                                       + Theme.tightSpacing
                                       + headerId.height
                                       + treeViewId.rows * Theme.treeRowHeight

    //The rows the view draws right now, the rows under rootNode included only
    //while their node is open.
    readonly property int rowCount: treeViewId.rows

    //The column the rows are ordered by, -1 while they stand in the order each
    //node lists its children and trips in. The header is what moves it, and
    //assigning -1 puts the rows back in their node's own order.
    property alias sortColumn: filterModelId.sortColumn

    //Which end of that column's values comes first, a Qt.SortOrder.
    property alias sortOrder: filterModelId.sortOrder

    //The rows the user picked. A verb on a row acts on the selection when the
    //row is part of it, which is what tripsFor() decides.
    readonly property ItemSelectionModel selectionModel: selectionModelId

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
        return RegionSurveyTree.objectFor(filterModelId.mapToSource(proxyIndex));
    }

    //The model index of the view row \a row, in the filter's coordinates.
    function indexAtRow(row: int) : var {
        return treeViewId.index(row, SurveyTreeModel.Name);
    }

    //The cwSurveyNode or cwTrip at a view row, null when the row is gone.
    function objectAtRow(row: int) : QQ.QtObject {
        return surveyTreeId.objectAt(surveyTreeId.indexAtRow(row));
    }

    //The row rootNode stands for, the invalid index for the whole region: the
    //model answers an unknown object with an invalid index, so a null rootNode
    //reads as the model's own root.
    function rootProxyIndex() : var {
        const sourceIndex = RegionSurveyTree.indexOf(surveyTreeId.rootNode);
        //A rootNode assigned while this view is still being built arrives
        //before the filter has its source model, and a mapping asked of the
        //filter then is a mapping of another model's index. The root is
        //applied again once the view is complete.
        if(filterModelId.sourceModel !== RegionSurveyTree) {
            return RegionSurveyTree.indexOf(null);
        }
        return filterModelId.mapFromSource(sourceIndex);
    }

    //Points the view back at rootNode's row whenever it has drifted off it. A
    //reset throws every index away and the row can arrive after the tree was
    //pointed at it, so every structural change of the model asks this. Whether
    //the view still stands on rootNode is asked of the object the rootIndex
    //names rather than of the index itself: an index left over from a model
    //that has been reset still reads as valid while naming nothing.
    function ensureRootIndex() {
        if(surveyTreeId.objectAt(treeViewId.rootIndex) !== surveyTreeId.rootNode) {
            surveyTreeId.applyRootIndex();
        }
    }

    //Points the view at rootNode's row. The filter rebuilds its indexes on
    //every filter change and on a reset, so the node is mapped afresh each
    //time rather than a stored index being trusted.
    function applyRootIndex() {
        //An invalid rootIndex is the model's own root, so a root the filter
        //has no row for would put the whole region on a cave's page. The view
        //gives up its model instead and shows nothing until the row is back,
        //which every structural change asks about through ensureRootIndex().
        const rootIndex = surveyTreeId.rootNode !== null
                        ? surveyTreeId.rootProxyIndex()
                        : undefined;
        const rowsAreShowable = surveyTreeId.rootNode === null || rootIndex.valid;

        //Assigned only when it changes: handing TableView a model rebuilds
        //every row, which throws away what the view has laid out.
        surveyTreeId.setViewModel(rowsAreShowable ? filterModelId : null);
        treeViewId.rootIndex = rowsAreShowable ? rootIndex : undefined;
        treeViewId.forceLayout();
    }

    //Points the view at \a model, leaving it alone when it has that model
    //already.
    function setViewModel(model: QQ.QtObject) {
        if(treeViewId.model !== model) {
            treeViewId.model = model;
        }
    }

    //The trips a row's verb acts on: the selected rows' trips when the row is
    //one of them, and the row's own trip otherwise — the rule the cave page's
    //trip table followed.
    function tripsFor(object: QQ.QtObject) : list<Trip> {
        const trip = object as Trip;
        if(trip === null) {
            return [];
        }

        const selected = surveyTreeId.selectedTrips();
        if(selected.length > 1 && selected.indexOf(trip) >= 0) {
            return selected;
        }
        return [trip];
    }

    //The calibrations of tripsFor(), which is what a calibration verb takes.
    function tripCalibrationsFor(object: QQ.QtObject) : list<TripCalibration> {
        return surveyTreeId.tripsFor(object).map(trip => trip.calibration);
    }

    //The trips of the selected rows. A row is selected one cell at a time, so
    //the same trip arrives once per column and is counted once.
    function selectedTrips() : list<Trip> {
        let trips = [];
        const indexes = selectionModelId.selectedIndexes;
        for(let i = 0; i < indexes.length; i++) {
            const trip = surveyTreeId.objectAt(indexes[i]) as Trip;
            if(trip !== null && trips.indexOf(trip) < 0) {
                trips.push(trip);
            }
        }
        return trips;
    }

    //Makes the first top-level row current as soon as the tree has rows, the
    //way the cave list this replaced made its first cave current: the page's
    //export verbs read the current row.
    function makeFirstRowCurrent() {
        const parentIndex = surveyTreeId.rootProxyIndex();
        if(selectionModelId.currentIndex.valid || filterModelId.rowCount(parentIndex) === 0) {
            return;
        }

        selectionModelId.setCurrentIndex(filterModelId.index(0, SurveyTreeModel.Name, parentIndex),
                                         ItemSelectionModel.NoUpdate);
    }

    //Opens every ancestor of \a object so its row is one of the view's rows.
    function expandTo(object: QQ.QtObject) {
        const sourceIndex = RegionSurveyTree.indexOf(object);
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
    //A filter shows its matches by opening the rows that hold them.
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
        //The filter rebuilds every index, so the view is pointed at its root
        //again before any row is asked for.
        surveyTreeId.applyRootIndex();

        if(isFiltering) {
            surveyTreeId.expandAll();
        } else if(wasFiltering) {
            surveyTreeId.collapseAll();
            surveyTreeId.restoreExpansion(surveyTreeId.expansionBeforeFilter);
            surveyTreeId.expansionBeforeFilter = [];
        }
    }

    //Orders the rows by the column a header cell stands for: the first ask
    //orders them by it, and asking again for the column they are already
    //ordered by flips which end comes first. Each node's own order is the
    //default rather than a state a click reaches, so no click clears the sort.
    //
    //The order is set ahead of the column so that flipping the order of the
    //column in hand reorders the rows once: the column set that follows it
    //finds the column it already has and does nothing.
    function toggleSort(column: int) {
        const ascending = !(filterModelId.sortColumn === column
                            && filterModelId.sortOrder === Qt.AscendingOrder);

        //What was open stays open: sorting announces a layout change, which
        //moves the rows the view has and keeps their expansion.
        filterModelId.sortOrder = ascending ? Qt.AscendingOrder : Qt.DescendingOrder;
        filterModelId.sortColumn = column;
        treeViewId.forceLayout();
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

    //The keys TableView leaves to the tree: → ← Space, Enter, F2, and the two
    //ways to ask for the context menu. ↑ ↓ Home End stay TableView's own.
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
        case Qt.Key_F2: {
            //The platform's rename key opens the current row's own name editor,
            //which is where a name is edited: a click on the name opens the row.
            const renameItem = surveyTreeId.currentRowItem();
            if(renameItem !== null && renameItem.canRename) {
                renameItem.startRename();
            }
            event.accepted = true;
            break;
        }
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

    onRootNodeChanged: {
        //One cave page item serves every cave, so the row that was current and
        //the rows that were picked belong to the cave that was showing. They
        //are dropped before the new root is applied, which keeps the page's
        //verbs — export, the calibration menu — on rows of the cave the page
        //now shows.
        selectionModelId.clearCurrentIndex();
        selectionModelId.clearSelection();
        surveyTreeId.applyRootIndex();
        surveyTreeId.makeFirstRowCurrent();
    }

    //A tree built over a region that already holds caves sees no insert.
    QQ.Component.onCompleted: {
        surveyTreeId.applyRootIndex();
        surveyTreeId.makeFirstRowCurrent();
    }

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

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.delegatePadding
                anchors.rightMargin: Theme.delegatePadding
                anchors.verticalCenter: parent.verticalCenter

                spacing: Theme.tightSpacing

                QC.Label {
                    id: headerLabelId

                    text: headerCellId.display
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeSmall
                    elide: QQ.Text.ElideRight

                    Layout.fillWidth: true
                }

                //The arrow stands on the one column the rows are ordered by,
                //and points at the end of its values that comes first.
                Icon {
                    objectName: "surveyTreeSortIndicator" + headerCellId.column

                    visible: filterModelId.sortColumn === headerCellId.column
                    source: filterModelId.sortOrder === Qt.AscendingOrder
                            ? "qrc:/twbs-icons/icons/sort-up.svg"
                            : "qrc:/twbs-icons/icons/sort-down.svg"
                    sourceSize: Qt.size(Theme.treeSortIndicatorSize, Theme.treeSortIndicatorSize)
                    colorizationColor: Theme.textSubtle
                }
            }

            //The header is the one sort control the tree has, in both the
            //Data page's layout and the cave page's.
            QQ.TapHandler {
                onTapped: surveyTreeId.toggleSort(headerCellId.column)
            }
        }
    }

    //The rows this view shows, filtered by its own toolbar. The tree below
    //gives up its model while its root has no row, so the proxy is held here
    //rather than inside the view.
    SurveyTreeFilterModel {
        id: filterModelId

        //The project's one tree: rooting and filtering are this view's, the
        //rows and their per-trip tasks are shared with every other tree.
        //The project's one tree: rooting and filtering are this view's, the
        //rows and their per-trip tasks are shared with every other tree.
        sourceModel: RegionSurveyTree
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

        //What the Name column gives up to the fixed columns beside it, asked of
        //the same provider that lays them out, so a column added here is added
        //in one place.
        readonly property int fixedColumnsWidth: {
            let total = 0;
            for(let column = 0; column < RegionSurveyTree.columnCount(); column++) {
                if(column !== SurveyTreeModel.Name) {
                    total += treeViewId.columnWidthProvider(column);
                }
            }
            return total;
        }

        clip: true
        keyNavigationEnabled: true
        pointerNavigationEnabled: true
        interactive: surveyTreeId.scrollable
        selectionBehavior: QQ.TableView.SelectRows
        //TableView's own ctrl and shift clicks then pick several rows, which
        //is what a verb over more than one trip acts on.
        selectionMode: QQ.TableView.ExtendedSelection
        //Enter opens the current row and renaming is the row's own editor, so
        //no key or tap starts a cell edit.
        editTriggers: QQ.TableView.NoEditTriggers

        Layout.fillWidth: true
        Layout.fillHeight: true

        //Assigned by applyRootIndex() rather than bound: a view whose root
        //has no row gives up its model, which is how it shows nothing.
        model: filterModelId

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
            case SurveyTreeModel.Stations:
                return Theme.treeStationsColumnWidth;
            case SurveyTreeModel.Decl:
                return Theme.treeDeclColumnWidth;
            case SurveyTreeModel.Length:
            case SurveyTreeModel.Depth:
                return Theme.treeStatColumnWidth;
            case SurveyTreeModel.Date:
                return Theme.treeDateColumnWidth;
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
                //The row the tree is rooted at can be one of these.
                surveyTreeId.ensureRootIndex();

                for(let row = first; row <= last; row++) {
                    const proxyIndex = filterModelId.index(row, SurveyTreeModel.Name, parent);
                    if(RegionSurveyTree.isSourceRootIndex(filterModelId.mapToSource(proxyIndex))) {
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

            function onRowsRemoved(parent, first, last) {
                surveyTreeId.ensureRootIndex();
            }

            function onModelReset() {
                //A reset throws every index away, rootIndex included; the rows
                //the root is looked up in arrive afterwards, which is what
                //onRowsInserted answers for.
                surveyTreeId.applyRootIndex();
                surveyTreeId.makeFirstRowCurrent();
            }

            function onLayoutChanged() {
                surveyTreeId.ensureRootIndex();
            }
        }
    }
}
