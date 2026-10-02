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
            // A sort is view state on the cached page item, so a test that
            // picked a sort — or failed part way through picking one — must not
            // hand it to the next test. The tree carries no objectName, so it
            // is reached through the filter field it owns.
            const page = RootData.pageView.currentPageItem
            let item = page !== null ? findChild(page, "surveyTreeFilter") : null
            while (item !== null && item.sortColumn === undefined) {
                item = item.parent
            }
            if (item !== null) {
                item.sortColumn = -1
            }

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

            const titles = ["Name", "Kind", "Trips", "Stations", "Length", "Depth", "Date", "Declination"]
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

        // The toolbar offers the filter alone: the carets and the keyboard are
        // how rows are opened and closed.
        function test_toolbarHasNoExpandOrCollapseAllButtons() {
            addCave("Alpha Cave", 2)

            const page = gotoDataMainPage()
            surveyTree(page)

            verify(findChild(page, "expandAllButton") === null,
                   "Expand all is gone")
            verify(findChild(page, "collapseAllButton") === null,
                   "Collapse all is gone")
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

            //A click opens the row and the name cell renames it, so a cave's
            //menu holds the one verb no cell carries.
            compare(menu.count, 1, "a cave row's menu offers Delete… alone")
            const deleteItem = findChild(menu, "surveyItemDeleteMenuItem")
            verify(deleteItem !== null && deleteItem.visible, "Delete… must be offered")
            compare(deleteItem.text, "Delete…")
            verify(findChild(menu, "surveyItemOpenMenuItem") === null,
                   "Open is gone: a click on the name opens the row")
            verify(findChild(menu, "surveyItemRenameMenuItem") === null,
                   "Rename is gone: the name cell renames in place")

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

            //Only a trip carries a calibration, so only a trip's menu offers
            //one — the cave page's trip table was the last place to set it.
            compare(menu.count, 2, "a trip row's menu offers Delete… and Declination")
            const submenu = menu.menuAt(1)
            verify(submenu !== null, "the second entry is a submenu")
            compare(submenu.objectName, "declinationSubmenu")

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
        // page's title uses. F2 is the platform's rename key, and it is what
        // asks the current row's name cell for its editor.
        function test_f2RenamesACaveInPlace() {
            addCave("Alpha Cave", 1)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)
            focusTree(tree)

            keyClick(Qt.Key_F2)

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

        // A helper for the cells that name their parts: a stat cell draws a
        // "value" and, where there is one, the "unit" beside it.
        function cellPart(page, cellObjectName, partObjectName) {
            let part = null
            tryVerify(() => {
                          const cell = findChild(page, cellObjectName)
                          part = cell === null ? null : findChild(cell, partObjectName)
                          return part !== null
                      }, 5000, cellObjectName + " must draw its " + partObjectName)
            return part
        }

        function cellValue(page, cellObjectName) {
            return cellPart(page, cellObjectName, "value")
        }

        // Waits for the named part of a cell to read the expected text. The
        // cell is looked up again on every poll, so a cell the view rebuilds
        // while its number is still being computed is picked up fresh.
        function tryCellText(page, cellObjectName, partObjectName, expected, message) {
            tryVerify(() => {
                          const cell = findChild(page, cellObjectName)
                          const part = cell === null ? null : findChild(cell, partObjectName)
                          return part !== null && part.text === expected
                      }, 10000,
                      message + ": " + cellObjectName + "->" + partObjectName
                      + " must read " + expected)
        }

        // One shot in one chunk: two stations, ten units of passage.
        function addShotToTrip(trip) {
            trip.addNewChunk()
            const chunk = trip.chunk(0)
            chunk.setData(SurveyChunk.StationNameRole, 0, "1")
            chunk.setData(SurveyChunk.StationNameRole, 1, "2")
            chunk.setData(SurveyChunk.ShotDistanceRole, 0, "10")
            chunk.setData(SurveyChunk.ShotCompassRole, 0, "0")
            chunk.setData(SurveyChunk.ShotClinoRole, 0, "0")
        }

        // A trip row carries its own surveyed length with the unit beside it,
        // its station count, and its declination — the three cells the cave
        // page's trip table used to be the only place to read. The length and
        // the count arrive from their tasks, so both are waited for.
        function test_tripRowShowsStationsLengthAndDeclination() {
            const cave = addCave("Alpha Cave", 1)
            const trip = cave.trip(0)
            trip.name = "Trip A"
            addShotToTrip(trip)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            tree.expand(0)
            tryCompare(tree, "rows", 2, 5000)

            //The trip names its stations the way the cave page's trip table
            //named them: the abbreviated range of stations 1 and 2.
            tryCellText(page, "tripStations1", "value", "1-2", "a trip names its own stations")

            //The cave's own count is read from the model rather than from its
            //cell: a cave row drawn before its trips' tasks report depends on
            //TreeView handing the cell the model's dataChanged, and a loaded
            //machine drops that refresh often enough to make the cell an
            //unreliable witness to the fold.
            const model = tree.model
            tryVerify(() => model.data(model.index(0, SurveyTreeModel.Stations),
                                       SurveyTreeModel.StationCountRole) === 2,
                      10000, "a cave counts the stations under it")

            //Utils.fixed drops a trailing zero run, the way the cave page shows a length.
            tryCellText(page, "tripLength1", "value", "10", "the trip's length is the number its task added up")

            //A trip's length reads in the unit the project displays, the same
            //unit its cave's length reads in, so one column holds one kind of
            //number.
            const tripMeters = Units.convertLength(10, trip.calibration.distanceUnit, Units.Meters)
            const displayUnit = Units.lengthDisplayUnit(tripMeters, ProjectUnits.unitSystem)
            tryCellText(page, "tripLength1", "unit", Units.lengthUnitName(displayUnit),
                        "the unit sits in the Length cell, beside the value")
            tryCellText(page, "caveLength0", "unit", Units.lengthUnitName(displayUnit),
                        "a cave and the trips under it name the same unit")

            compare(cellValue(page, "tripDecl1").text, "0°")
            //The Decl cell says how the angle was picked.
            compare(cellPart(page, "tripDecl1", "declinationMode").text, "manual",
                    "a trip with no fix station has no automatic declination")
        }

        // A cave's Length cell reads the solved length with its unit; its Decl
        // and Date cells are a trip's business alone.
        function test_nodeRowShowsLengthWithItsUnitAndNoDateOrDeclination() {
            const cave = addCave("Alpha Cave", 1)
            cave.trip(0).date = new Date(2026, 8, 15)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            //The node's length is solved after the page is shown, so its unit
            //is waited for rather than read on the first frame.
            tryCellText(page, "caveLength0", "unit",
                        Units.lengthUnitName(Units.lengthDisplayUnit(0.0, ProjectUnits.unitSystem)),
                        "the unit sits in the Length cell, beside the value")

            compare(cellValue(page, "caveDate0").text, "",
                    "a cave holds trips surveyed on many days, so its Date cell is empty")
            compare(cellValue(page, "caveStations0").text, "0",
                    "a cave counts the stations under it rather than naming them")
            verify(findChild(page, "caveDecl0") === null
                   || findChild(findChild(page, "caveDecl0"), "value") === null,
                   "only a trip carries a declination")

            tree.expand(0)
            tryCompare(tree, "rows", 2, 5000)
            tryCompare(cellValue(page, "tripDate1"), "text", "2026-09-15", 5000)
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

        // --- C2b.5: the header is the tree's sort control ---

        function headerCell(page, column) {
            let cell = null
            tryVerify(() => {
                          cell = findChild(page, "surveyTreeHeaderCell" + column)
                          return cell !== null
                      }, 5000, "header cell " + column + " must exist")
            return cell
        }

        //The names of the view's rows in the order they stand in. surveyTree()
        //hands back the TreeView itself, which reaches the SurveyTreeView
        //around it the way its own delegates do.
        function treeRowNames(tree) {
            let names = []
            for (let row = 0; row < tree.rows; row++) {
                names.push(String(tree.surveyTree.objectAtRow(row).name))
            }
            return names
        }

        // A click on the Name header orders the caves by name, a second click
        // turns the order around, and the arrow says which column is sorted.
        function test_headerClickSortsTheRowsAndShowsTheIndicator() {
            addCave("Beta Cave", 0)
            addCave("Alpha Cave", 0)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)

            //The rows start in each node's own order, which is what -1 means
            tree.surveyTree.sortColumn = -1
            tryVerify(() => treeRowNames(tree).join() === "Beta Cave,Alpha Cave",
                      5000, "the region lists the caves the way they were added")

            const nameHeader = headerCell(page, SurveyTreeModel.Name)
            const indicator = findChild(page, "surveyTreeSortIndicator" + SurveyTreeModel.Name)
            verify(indicator !== null, "every header cell carries an arrow")
            verify(!indicator.visible, "which stays out of sight until its column sorts")

            mouseClick(nameHeader)
            tryVerify(() => treeRowNames(tree).join() === "Alpha Cave,Beta Cave",
                      5000, "the Name header orders the caves by name")
            compare(tree.surveyTree.sortColumn, SurveyTreeModel.Name)
            compare(tree.surveyTree.sortOrder, Qt.AscendingOrder)
            tryVerify(() => indicator.visible, 5000, "the arrow stands on the sorted column")
            verify(!findChild(page, "surveyTreeSortIndicator" + SurveyTreeModel.Trips).visible,
                   "and on no other column")

            mouseClick(nameHeader)
            tryVerify(() => treeRowNames(tree).join() === "Beta Cave,Alpha Cave",
                      5000, "clicking the same header turns the order around")
            compare(tree.surveyTree.sortOrder, Qt.DescendingOrder)
            tryVerify(() => indicator.visible, 5000, "the arrow stays, pointing the other way")
        }

        // Sorting moves the rows around; the caves that were open stay open.
        function test_expansionSurvivesASort() {
            const beta = addCave("Beta Cave", 2)
            beta.trip(0).name = "Trip A"
            beta.trip(1).name = "Trip B"
            addCave("Alpha Cave", 0)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)
            //The page is cached between tests, so the rows start in the order
            //each node lists them in
            tree.surveyTree.sortColumn = -1

            //Beta is the first row until the sort moves it
            tree.expand(0)
            tryCompare(tree, "rows", 4, 5000)
            verify(tree.isExpanded(0), "Beta Cave is open")

            mouseClick(headerCell(page, SurveyTreeModel.Name))
            tryVerify(() => treeRowNames(tree)[0] === "Alpha Cave",
                      5000, "sorting by name puts Alpha Cave first")

            tryCompare(tree, "rows", 4, 5000)
            verify(tree.isExpanded(1), "Beta Cave is still open where the sort put it")
            compare(treeRowNames(tree), ["Alpha Cave", "Beta Cave", "Trip A", "Trip B"],
                    "and its trips are still the rows under it")
        }

        // --- C4c.2: the row's badges read out their messages ---

        function addUtm13NFix(cave, name, easting) {
            cave.fixStations.addFixStation()
            const index = cave.fixStations.index(cave.fixStations.count - 1)
            cave.fixStations.setData(index, name, FixStationModel.StationNameRole)
            cave.fixStations.setData(index, "EPSG:32613", FixStationModel.InputCSRole)
            cave.fixStations.setData(index, easting, FixStationModel.EastingRole)
            cave.fixStations.setData(index, 4430000.0, FixStationModel.NorthingRole)
            cave.fixStations.setData(index, 1655.0, FixStationModel.ElevationRole)
        }

        function errorIconBar(page, rowObjectName) {
            let bar = null
            tryVerify(() => {
                          const row = findChild(page, rowObjectName)
                          bar = row === null ? null : findChild(row, "errorIconBar")
                          return bar !== null
                      }, 5000, rowObjectName + " must carry its error badges")
            return bar
        }

        // The message labels the open popover lists, by their text.
        function popoverMessages(popover) {
            const messages = []
            const visit = (item) => {
                if(item.objectName === "errorMessage" && item.visible) {
                    messages.push(item.text)
                }
                for(let i = 0; i < item.children.length; i++) {
                    visit(item.children[i])
                }
            }
            visit(popover.contentItem)
            return messages
        }

        function test_nodeWarningBadgeOpensItsMessages() {
            const warned = addCave("Warned Cave", 0)
            addCave("Clean Cave", 0)

            //One fix in its domain gives the project a frame, and a transposed
            //leading digit puts the second outside UTM 13N's valid range.
            addUtm13NFix(warned, "anchor", 478000.0)
            addUtm13NFix(warned, "BAD", 1478000.0)
            tryVerify(() => warned.errorModel.warningCount > 0, 5000,
                      "the out-of-domain fix warns on its cave")

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)
            tree.surveyTree.sortColumn = -1
            tryVerify(() => treeRowNames(tree).join() === "Warned Cave,Clean Cave",
                      5000, "the caves keep the order they were added in")

            const warnedBar = errorIconBar(page, "caveDelegate0")
            const cleanBar = errorIconBar(page, "caveDelegate1")

            tryVerify(() => findChild(warnedBar, "warningBadge").visible, 5000,
                      "a node with a warning shows the warning badge")
            verify(!findChild(warnedBar, "noErrorBadge").visible)

            verify(!findChild(cleanBar, "warningBadge").visible,
                   "a node without a warning has no warning badge")
            verify(!findChild(cleanBar, "fatalBadge").visible,
                   "and no fatal badge")
            verify(findChild(cleanBar, "noErrorBadge").visible,
                   "it keeps the good icon instead")

            const popover = findChild(warnedBar, "errorPopover")
            verify(popover !== null, "the badges carry a popover")
            verify(!popover.opened)

            mouseClick(warnedBar)
            tryVerify(() => popover.opened, 5000, "a tap opens the messages")
            verify(warnedBar.pinned, "and pins them open")
            tryVerify(() => popover.activeFocus, 5000, "a pinned popover takes focus for Escape")

            let messages = []
            tryVerify(() => {
                          messages = popoverMessages(popover)
                          return messages.length > 0
                      }, 5000, "the popover lists the node's messages")
            verify(messages.some(message => message.indexOf("BAD") >= 0),
                   "the list names the offending station: " + messages.join(" | "))

            keyClick(Qt.Key_Escape)
            tryVerify(() => !popover.opened, 5000, "Escape closes the popover")
            verify(!warnedBar.pinned)

            mouseClick(warnedBar)
            tryVerify(() => popover.opened, 5000, "a second tap opens it again")
            mouseClick(page, page.width / 2, page.height - Theme.treeRowHeight)
            tryVerify(() => !popover.opened, 5000, "a press elsewhere closes the popover")

            const cleanPopover = findChild(cleanBar, "errorPopover")
            mouseClick(cleanBar)
            verify(!cleanPopover.opened, "a node without a message has nothing to open")
        }

        function test_hoveringTheBadgePeeksTheMessagesOpen() {
            const warned = addCave("Warned Cave", 0)
            addUtm13NFix(warned, "anchor", 478000.0)
            addUtm13NFix(warned, "BAD", 1478000.0)
            tryVerify(() => warned.errorModel.warningCount > 0, 5000,
                      "the out-of-domain fix warns on its cave")

            const page = gotoDataMainPage()
            const bar = errorIconBar(page, "caveDelegate0")
            const popover = findChild(bar, "errorPopover")

            mouseMove(bar)
            tryVerify(() => popover.opened, 5000, "hovering the badges peeks the popover open")
            verify(!bar.pinned, "a peek does not pin it")

            mouseMove(page, page.width / 2, page.height - Theme.treeRowHeight)
            tryVerify(() => !popover.opened, 5000, "leaving the badges closes the peek")
        }
    }
}
