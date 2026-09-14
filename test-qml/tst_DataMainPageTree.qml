import QtQuick as QQ
import QtQuick.Controls as QC
import QtQml.Models
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    // The smoke gate's bare TreeView. It lives in a Loader so it only exists
    // while the smoke test runs — the page tests below click through
    // mainWindow, which this would otherwise cover.
    QQ.Loader {
        id: smokeLoaderId
        active: false
        width: 400
        height: 300
        sourceComponent: smokeComponentId
    }

    QQ.Component {
        id: smokeComponentId

        QQ.TreeView {
            id: smokeTreeId
            objectName: "smokeTree"
            anchors.fill: parent
            clip: true

            model: SurveyTreeFilterModel {
                sourceModel: SurveyTreeModel {
                    region: RootData.region
                }
            }

            selectionModel: ItemSelectionModel {
                model: smokeTreeId.model
            }

            delegate: QQ.Rectangle {
                id: smokeDelegateId

                required property string name
                required property int depth

                implicitWidth: 200
                implicitHeight: 24
                color: Theme.surface

                QC.Label {
                    x: smokeDelegateId.depth * 10
                    text: smokeDelegateId.name
                }
            }
        }
    }

    TestCase {
        name: "DataMainPageTree"
        when: windowShown

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
        }

        function cleanup() {
            rootId.closeAnyOpenEditor()
            smokeLoaderId.active = false
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        function addCave(name, tripCount) {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.rowCount() - 1)
            cave.name = name
            for(let i = 0; i < tripCount; i++) {
                cave.addTrip()
            }
            return cave
        }

        // Smoke gate: a stock TreeView over the new models draws rows offscreen
        // and its expand/collapse change the view's row count.
        function test_treeViewShowsAndExpandsRowsOffscreen() {
            addCave("Alpha Cave", 2)
            addCave("Beta Cave", 1)

            smokeLoaderId.active = true
            const treeView = smokeLoaderId.item
            verify(treeView !== null, "the smoke TreeView must load")
            waitForRendering(treeView)

            tryCompare(treeView, "rows", 2, 5000)

            treeView.expand(0)
            tryCompare(treeView, "rows", 4, 5000)

            treeView.collapse(0)
            tryCompare(treeView, "rows", 2, 5000)
        }

        function gotoDataMainPage() {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "should land on dataMainPage")
            const page = RootData.pageView.currentPageItem
            waitForRendering(page)
            return page
        }

        // The TreeView carries no objectName — a name between the page and a
        // row would break the chains the other Data page tests match — so the
        // view is reached the way its own delegates see it.
        function surveyTree(page) {
            let row = null
            tryVerify(() => {
                          row = findChild(page, "caveDelegate0")
                          return row !== null
                      }, 5000, "the Data page must show the first cave row")
            return row.treeView
        }

        // Every column title comes from the model's headerData through the
        // stock HorizontalHeaderView, which #502 broke over a list-shaped
        // model — this is the tree-shaped model's proof.
        function test_headerShowsEveryColumnTitle() {
            addCave("Alpha Cave", 1)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            const titles = ["Name", "Kind", "Trips", "Length", "Depth", "Last survey", ""]
            for(let column = 0; column < titles.length; column++) {
                let cell = null
                tryVerify(() => {
                              cell = findChild(page, "surveyTreeHeaderCell" + column)
                              return cell !== null
                          }, 5000, "header cell " + column + " must exist")
                compare(cell.display, titles[column],
                        "header cell " + column + " must carry its own title")
            }
        }

        function test_caveRowsAreCollapsedAndReadCave() {
            addCave("Alpha Cave", 2)
            addCave("Beta Cave", 1)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)

            //Two caves, no trip rows: a native cave starts collapsed.
            tryCompare(tree, "rows", 2, 5000)

            for(let row = 0; row < 2; row++) {
                let kindCell = null
                tryVerify(() => {
                              kindCell = findChild(page, "caveKind" + row)
                              return kindCell !== null
                          }, 5000, "row " + row + " must show a Kind cell")
                const chip = findChild(kindCell, "kindChip")
                verify(chip !== null, "row " + row + " must show a kind chip")
                compare(chip.text, "Cave")
            }

            verify(findChild(page, "caveDelegate0") !== null,
                   "the first cave row keeps the caveDelegate0 name")
            verify(findChild(page, "tripDelegate1") === null,
                   "a collapsed cave shows no trip rows")
        }

        function test_expandingACaveShowsItsTrips() {
            const cave = addCave("Alpha Cave", 2)
            cave.trip(0).name = "Trip A"
            cave.trip(1).name = "Trip B"
            addCave("Beta Cave", 1)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)

            tree.expand(0)
            tryCompare(tree, "rows", 4, 5000)

            let tripRow = null
            tryVerify(() => {
                          tripRow = findChild(page, "tripDelegate1")
                          return tripRow !== null
                      }, 5000, "the first trip must take the row under its cave")
            const tripLink = findChild(tripRow, "tripLink")
            verify(tripLink !== null, "a trip row shows its name as a link")
            compare(tripLink.text, "Trip A")

            tree.collapse(0)
            tryCompare(tree, "rows", 2, 5000)
        }

        function test_clickingATripRowOpensTheTripPageAndBackKeepsExpansion() {
            const cave = addCave("Alpha Cave", 2)
            cave.trip(0).name = "Trip A"
            cave.trip(1).name = "Trip B"

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            tree.expand(0)
            tryCompare(tree, "rows", 3, 5000)

            let tripLink = null
            tryVerify(() => {
                          const tripRow = findChild(page, "tripDelegate2")
                          tripLink = tripRow === null ? null : findChild(tripRow, "tripLink")
                          return tripLink !== null
                      }, 5000, "the second trip row must show its link")
            compare(tripLink.text, "Trip B")

            mouseClick(tripLink)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "tripPage",
                      5000, "clicking a trip row must land on the trip page")

            RootData.pageSelectionModel.back()
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "back must land on the Data page again")

            //The Data page is cached, so its tree keeps the rows it had open.
            const treeAgain = surveyTree(RootData.pageView.currentPageItem)
            tryCompare(treeAgain, "rows", 3, 5000)
            verify(treeAgain.isExpanded(0), "the cave must still be expanded")
        }

        // --- C2.3: filter, keyboard, and the row menus ---

        // The row's own menu: one per row, carried by its Name cell, so it is
        // found under the row rather than under the page.
        function rowContextMenu(page, rowObjectName) {
            const row = findChild(page, rowObjectName)
            verify(row !== null, rowObjectName + " must exist")
            let menu = null
            tryVerify(() => {
                          menu = findChild(row, "surveyItemContextMenu")
                          return menu !== null && menu.visible
                      }, 5000, rowObjectName + " must open its context menu")
            return menu
        }

        function filterField(page) {
            const field = findChild(page, "surveyTreeFilter")
            verify(field !== null, "the tree's toolbar must show a filter field")
            return field
        }

        // Typing hides every row that neither matches nor holds a match, and
        // clearing puts back exactly what was open before. Qt promises nothing
        // about expansion surviving the proxy's rebuild, so this is the
        // contract for the view's snapshot and restore.
        function test_filterShowsMatchesAndClearingRestoresExpansion() {
            const alpha = addCave("Alpha Cave", 2)
            alpha.trip(0).name = "Trip A"
            alpha.trip(1).name = "Trip B"
            const beta = addCave("Beta Cave", 1)
            beta.trip(0).name = "Gamma Trip"

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)

            tree.expand(0)
            tryCompare(tree, "rows", 4, 5000)

            const filter = filterField(page)
            filter.text = "beta"
            tryCompare(tree, "rows", 1, 5000)

            let onlyRow = null
            tryVerify(() => {
                          onlyRow = findChild(page, "caveDelegate0")
                          return onlyRow !== null
                      }, 5000, "the matching cave must keep a row")
            compare(onlyRow.name, "Beta Cave", "only the matching cave is shown")

            //A matching trip carries its cave along, expanded.
            filter.text = "gamma"
            tryCompare(tree, "rows", 2, 5000)
            let tripRow = null
            tryVerify(() => {
                          tripRow = findChild(page, "tripDelegate1")
                          return tripRow !== null
                      }, 5000, "the matching trip must show under its cave")
            compare(tripRow.name, "Gamma Trip")

            filter.text = ""
            tryCompare(tree, "rows", 4, 5000)
            verify(tree.isExpanded(0), "clearing the filter restores what was open")
            verify(!tree.isExpanded(3), "a cave that was closed stays closed")
        }

        function test_expandAllAndCollapseAllButtons() {
            addCave("Alpha Cave", 2)
            addCave("Beta Cave", 1)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)

            const expandAll = findChild(page, "expandAllButton")
            verify(expandAll !== null, "Expand all must exist")
            mouseClick(expandAll)
            tryCompare(tree, "rows", 5, 5000)

            const collapseAll = findChild(page, "collapseAllButton")
            verify(collapseAll !== null, "Collapse all must exist")
            mouseClick(collapseAll)
            tryCompare(tree, "rows", 2, 5000)
        }

        function focusTree(tree) {
            tree.forceActiveFocus()
            tryVerify(() => tree.activeFocus, 5000, "the tree must take the keyboard")
            tryVerify(() => tree.currentRow === 0, 5000, "the first row starts current")
        }

        // → expands, ↓ walks into the trips, Enter opens the current row.
        function test_keyboardExpandsMovesAndOpens() {
            const cave = addCave("Alpha Cave", 2)
            cave.trip(0).name = "Trip A"
            cave.trip(1).name = "Trip B"

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)
            focusTree(tree)

            keyClick(Qt.Key_Right)
            tryCompare(tree, "rows", 3, 5000)

            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Down)
            tryCompare(tree, "currentRow", 2, 5000)

            keyClick(Qt.Key_Return)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "tripPage",
                      5000, "Enter must open the current row's page")
        }

        // ← collapses an open node, and on a row with nothing to close it
        // moves to the parent row.
        function test_keyboardLeftCollapsesThenSelectsTheParent() {
            const cave = addCave("Alpha Cave", 2)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)
            focusTree(tree)

            keyClick(Qt.Key_Right)
            tryCompare(tree, "rows", 3, 5000)

            keyClick(Qt.Key_Down)
            tryCompare(tree, "currentRow", 1, 5000)

            //A trip row has nothing to collapse, so ← walks up to its cave.
            keyClick(Qt.Key_Left)
            tryCompare(tree, "currentRow", 0, 5000)
            compare(tree.rows, 3, "moving to the parent leaves the rows alone")

            keyClick(Qt.Key_Left)
            tryCompare(tree, "rows", 1, 5000)
            verify(!tree.isExpanded(0), "← on an open node closes it")
        }

        function test_keyboardSpaceTogglesTheCurrentNode() {
            addCave("Alpha Cave", 2)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)
            focusTree(tree)

            keyClick(Qt.Key_Space)
            tryCompare(tree, "rows", 3, 5000)

            keyClick(Qt.Key_Space)
            tryCompare(tree, "rows", 1, 5000)
        }

        // Shift+F10 opens the current row's menu, and Delete… asks with the
        // trips it would take along before removing the cave.
        function test_contextMenuDeletesTheCaveWithACountedPrompt() {
            addCave("Alpha Cave", 2)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)
            focusTree(tree)

            keyClick(Qt.Key_F10, Qt.ShiftModifier)

            const menu = rowContextMenu(page, "caveDelegate0")

            const openItem = findChild(menu, "surveyItemOpenMenuItem")
            const renameItem = findChild(menu, "surveyItemRenameMenuItem")
            const deleteItem = findChild(menu, "surveyItemDeleteMenuItem")
            verify(openItem !== null && openItem.visible, "Open must be offered")
            verify(renameItem !== null && renameItem.visible,
                   "a native cave must be renamable")
            verify(deleteItem !== null && deleteItem.visible, "Delete… must be offered")

            mouseClick(deleteItem)

            let askBox = null
            tryVerify(() => {
                          askBox = findChild(page, "removeChallange")
                          return askBox !== null && askBox.visible
                      }, 5000, "Delete… must ask first")
            compare(askBox.message, "Remove <b>Alpha Cave</b> and its 2 trips?")

            const removeButton = findChild(askBox, "removeButton")
            verify(removeButton !== null, "the prompt must offer Remove")
            mouseClick(removeButton)

            tryCompare(tree, "rows", 0, 5000)
            compare(RootData.region.rowCount(), 0, "the cave is gone")
        }

        // A trip's prompt names the trip alone, and removing it takes the row
        // with it.
        function test_contextMenuDeletesATrip() {
            const cave = addCave("Alpha Cave", 2)
            cave.trip(0).name = "Trip A"
            cave.trip(1).name = "Trip B"

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            tree.expand(0)
            tryCompare(tree, "rows", 3, 5000)
            focusTree(tree)

            keyClick(Qt.Key_Down)
            tryCompare(tree, "currentRow", 1, 5000)

            keyClick(Qt.Key_F10, Qt.ShiftModifier)

            const menu = rowContextMenu(page, "tripDelegate1")

            verify(findChild(menu, "surveyItemRenameMenuItem") === null,
                   "a trip is renamed on its own page, not here")

            mouseClick(findChild(menu, "surveyItemDeleteMenuItem"))

            let askBox = null
            tryVerify(() => {
                          askBox = findChild(page, "removeChallange")
                          return askBox !== null && askBox.visible
                      }, 5000, "Delete… must ask first")
            compare(askBox.message, "Remove <b>Trip A</b>?")

            mouseClick(findChild(askBox, "removeButton"))

            tryCompare(tree, "rows", 2, 5000)
            compare(cave.tripCount, 1, "only the asked-about trip is gone")
        }

        // Rename edits the name in place, through the same editor the cave
        // page's title uses.
        function test_contextMenuRenamesACave() {
            addCave("Alpha Cave", 1)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)
            focusTree(tree)

            keyClick(Qt.Key_F10, Qt.ShiftModifier)

            const menu = rowContextMenu(page, "caveDelegate0")

            mouseClick(findChild(menu, "surveyItemRenameMenuItem"))

            tryVerify(() => rootId.shadowEditor.coreClickInput !== null, 5000,
                      "Rename must open the row's name editor")

            rootId.shadowEditor.setEditorText("Renamed Cave")
            rootId.shadowEditor.coreClickInput.commitChanges()

            tryCompare(RootData.region.cave(0), "name", "Renamed Cave", 5000)

            let caveRow = null
            tryVerify(() => {
                          caveRow = findChild(page, "caveDelegate0")
                          return caveRow !== null && caveRow.name === "Renamed Cave"
                      }, 5000, "the row must show the new name")
        }

        // The row's ⋯ menu adds a trip to that row's node and opens it, the way
        // the cave page's Add Trip does.
        function test_rowMenuAddsATrip() {
            const cave = addCave("Alpha Cave", 0)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            let actionsButton = null
            tryVerify(() => {
                          actionsButton = findChild(page, "rowActionsButton")
                          return actionsButton !== null
                      }, 5000, "the row must show its ⋯ button")
            mouseClick(actionsButton)

            let menu = null
            tryVerify(() => {
                          menu = findChild(actionsButton, "rowActionsMenu")
                          return menu !== null && menu.visible
                      }, 5000, "the ⋯ button must open the row menu")

            const addTripItem = findChild(menu, "rowAddTripMenuItem")
            verify(addTripItem !== null && addTripItem.visible, "Add Trip must be offered")
            mouseClick(addTripItem)

            tryCompare(cave, "tripCount", 1, 5000)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "tripPage",
                      5000, "Add Trip must open the new trip")

            RootData.pageSelectionModel.back()
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "back must land on the Data page again")

            const treeAgain = surveyTree(RootData.pageView.currentPageItem)
            tryCompare(treeAgain, "rows", 2, 5000)
        }

        // The Add ▾ caret holds the cave-level attach, under its settled name.
        function test_addCaretOffersAttachSurveyFile() {
            const page = gotoDataMainPage()

            const addBar = findChild(page, "addCave")
            verify(addBar !== null, "the Add bar must exist")
            const menuButton = findChild(addBar, "menuButton")
            verify(menuButton !== null, "the split button must show its caret")
            mouseClick(menuButton)

            let menu = null
            tryVerify(() => {
                          menu = findChild(page, "addCaveMenu")
                          return menu !== null && menu.visible
                      }, 5000, "the caret must open the Add menu")
            compare(menu.count, 1, "the Data page offers one extra way to add")
            const item = menu.itemAt(0)
            compare(item.objectName, "addExternalCaveMenuItem")
            compare(item.text, "Attach survey file…")
            menu.close()
        }
    }
}
