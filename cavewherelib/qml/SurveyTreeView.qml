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

    //A tree built over a region that already holds caves sees no insert.
    QQ.Component.onCompleted: surveyTreeId.makeFirstRowCurrent()

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
        //either one renames it there too.
        property LinkGenerator linkGenerator: linkGeneratorId
        property RemoveAskBox removeAskBox: surveyTreeId.removeAskBox

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
