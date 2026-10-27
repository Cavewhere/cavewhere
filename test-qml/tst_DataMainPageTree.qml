import QtQuick as QQ
import QtQuick.Controls as QC
import QtCore as QtCore
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

    readonly property list<string> treeColumnWidthKeys: ["kindColumnWidth", "tripsColumnWidth",
        "stationsColumnWidth", "lengthColumnWidth", "depthColumnWidth", "dateColumnWidth",
        "declColumnWidth"]

    // The survey tree's dragged column widths, shared by the Data page and
    // the node page. cleanup() puts back whatever a test dragged.
    QtCore.Settings {
        id: treeColumnSettingsId
        category: "SurveyTreeView"
    }

    TestCase {
        name: "DataMainPageTree"
        when: windowShown

        readonly property int defaultWindowWidth: 1200
        // Wide enough that the Name column stands above its minimum, so a
        // fixed column has room to widen.
        readonly property int wideWindowWidth: 1600

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
            RegionSurveyTree.cancelMove()
            smokeLoaderId.active = false
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()

            rootId.width = defaultWindowWidth
            for (const key of rootId.treeColumnWidthKeys) {
                treeColumnSettingsId.setValue(key, 0)
            }
            treeColumnSettingsId.sync()
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

        // The shared expand/collapse button, found on the Data page's tree.
        function surveyTreeToggle(page) {
            let button = null
            tryVerify(() => {
                          button = findChild(page, "surveyTreeExpandToggle")
                          return button !== null
                      }, 5000, "the survey tree's toolbar must hold the expand/collapse button")
            return button
        }

        // One toolbar button opens every row and closes them again; a single
        // closed row turns it back to Expand all, and a filter hides it.
        function test_oneButtonTogglesExpandAndCollapseAllRows() {
            addCave("Alpha Cave", 2)
            addCave("Beta Cave", 1)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)

            const button = surveyTreeToggle(page)
            tryVerify(() => button.visible, 5000, "the button shows while a row has children")
            tryCompare(button, "text", "Expand all", 5000, "collapsed caves make the button expand")
            verify(String(button.icon.source).indexOf("arrows-expand") >= 0,
                   "the button shows the expand icon")

            mouseClick(button)
            tryCompare(tree, "rows", 5, 5000, "expand all shows both caves and their three trips")
            tryCompare(button, "text", "Collapse all", 5000, "with every row open the button collapses")
            verify(String(button.icon.source).indexOf("arrows-collapse") >= 0,
                   "the icon swaps to the collapse icon")

            mouseClick(button)
            tryCompare(tree, "rows", 2, 5000, "collapse all leaves the two caves")
            tryCompare(button, "text", "Expand all")

            tree.expand(0)
            tryCompare(tree, "rows", 4, 5000)
            tryCompare(button, "text", "Expand all", 5000,
                       "one open cave beside a closed one keeps Expand all")

            mouseClick(button)
            tryCompare(tree, "rows", 5, 5000)
            tryCompare(button, "text", "Collapse all")
            tree.collapse(0)
            tryCompare(tree, "rows", 3, 5000)
            tryCompare(button, "text", "Expand all", 5000,
                       "one closed cave turns the button to Expand all")

            const filter = findChild(page, "surveyTreeFilter")
            filter.text = "beta"
            tryVerify(() => !button.visible, 5000, "a filter hides the button")
            filter.text = ""
            tryVerify(() => button.visible, 5000, "clearing the filter brings the button back")
        }

        // Rows without children leave nothing to open, so the button hides.
        function test_expandToggleHidesWhileNoRowHasChildren() {
            addCave("Alpha Cave", 0)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            const button = surveyTreeToggle(page)
            tryVerify(() => !button.visible, 5000, "a lone empty cave offers no expand/collapse")

            RootData.region.cave(0).addTrip()
            tryVerify(() => button.visible, 5000, "a trip under the cave brings the button")
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
            //menu holds the verbs no cell carries: Add, Move to… and Delete….
            compare(menu.count, 3, "a cave row's menu offers Add, Move to… and Delete…")
            compare(menu.menuAt(0).objectName, "addVerbsSubmenu")
            compare(menu.itemAt(1).objectName, "surveyItemMoveMenuItem", "Move to… follows Add")
            compare(menu.itemAt(2).objectName, "surveyItemDeleteMenuItem", "Delete… comes last")
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
            compare(askBox.message,
                    "Delete <b>Alpha Cave</b> and its 2 trips?")

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
            compare(menu.count, 3, "a trip row's menu offers Move to…, Delete… and Declination")
            const submenu = menu.menuAt(2)
            verify(submenu !== null, "the third entry is a submenu")
            compare(submenu.objectName, "declinationSubmenu")
            compare(menu.itemAt(0).objectName, "surveyItemMoveMenuItem", "Move to… leads")
            compare(menu.itemAt(1).objectName, "surveyItemDeleteMenuItem",
                    "Delete… sits between Move to… and Declination")

            mouseClick(findChild(menu, "surveyItemDeleteMenuItem"))

            let askBox = null
            tryVerify(() => {
                          askBox = findChild(page, "removeChallange")
                          return askBox !== null && askBox.visible
                      }, 5000, "Delete… must ask first")
            compare(askBox.message, "Delete <b>Trip A</b>?")

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

            compare(cellValue(page, "tripDecl1").text, "0.00°")
            //The Decl cell says how the angle was picked, before the angle.
            const declMode = cellPart(page, "tripDecl1", "declinationMode")
            compare(declMode.text, "manual",
                    "a trip with no fix station has no automatic declination")
            const declCell = findChild(page, "tripDecl1")
            verify(declMode.mapToItem(declCell, 0, 0).x
                   < cellValue(page, "tripDecl1").mapToItem(declCell, 0, 0).x,
                   "a native trip's caption reads before its angle too")
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

        function openAddCaret(page) {
            const addBar = findChild(page, "addCave")
            verify(addBar !== null, "the Add bar must exist")
            const menuButton = findChild(addBar, "menuButton")
            verify(menuButton !== null, "the split button must show its caret")
            mouseClick(menuButton)

            let menu = null
            tryVerify(() => {
                          menu = findChild(page, "addCaveMenu")
                          return menu !== null && menu.opened
                      }, 5000, "the caret must open the Add menu")
            return menu
        }

        // The objectNames of a menu's entries, in order.
        function menuEntryNames(menu) {
            const names = []
            for(let i = 0; i < menu.count; i++) {
                names.push(menu.itemAt(i).objectName)
            }
            return names
        }

        // --- C5.1: Add Folder / Add Section / the kind chip / delete counts ---

        // Opens the context menu of the row at view row \a viewRow and returns
        // it. The menu belongs to the current row, so the row is made current
        // first.
        function openRowMenu(page, tree, viewRow) {
            tree.surveyTree.setCurrentRow(viewRow)
            tree.forceActiveFocus()
            tryVerify(() => tree.activeFocus && tree.currentRow === viewRow, 5000,
                      "row " + viewRow + " must be current")
            keyClick(Qt.Key_F10, Qt.ShiftModifier)
            return rowContextMenu(page, "caveDelegate" + viewRow)
        }

        // Opens the Add submenu of a node row's context menu.
        function openAddSubmenu(menu) {
            const submenu = menu.menuAt(0)
            verify(submenu !== null && submenu.objectName === "addVerbsSubmenu",
                   "a node row's menu leads with Add")
            mouseClick(menu.itemAt(0))
            tryVerify(() => submenu.opened, 5000, "Add must open its verbs")
            return submenu
        }

        // A new node opens straight into its name editor; this names it.
        function commitNewName(name) {
            tryVerify(() => rootId.shadowEditor.coreClickInput !== null, 5000,
                      "a new node must open its name editor")
            rootId.shadowEditor.setEditorText(name)
            rootId.shadowEditor.coreClickInput.commitChanges()
        }

        function test_addFolderAtTopLevelOffersCavesAndFoldersInside() {
            const page = gotoDataMainPage()
            mouseClick(findChild(openAddCaret(page), "addFolderMenuItem"))

            tryCompare(RootData.region, "caveCount", 1, 5000)
            const folder = RootData.region.cave(0)
            compare(folder.kind, SurveyNodeKind.Folder, "Add Folder makes a Folder")
            compare(folder.name, "New Folder")
            verify(folder.takesCaves, "a top-level Folder holds caves")

            commitNewName("Kentucky field seasons")
            tryCompare(folder, "name", "Kentucky field seasons", 5000)
            compare(RootData.pageView.currentPageItem, page,
                    "a new folder keeps the Data page in front")

            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)
            const chip = findChild(findChild(page, "caveKind0"), "kindChip")
            compare(chip.text, "Folder")
            const icon = findChild(findChild(page, "caveDelegate0"), "kindIcon")
            verify(icon !== null && icon.visible, "a Folder row carries its kind icon")
            verify(String(icon.source).indexOf("folder.svg") >= 0,
                   "a top-level Folder draws the folder icon")

            const menu = openRowMenu(page, tree, 0)
            const submenu = openAddSubmenu(menu)
            compare(menuEntryNames(submenu),
                    ["addCaveMenuItem", "addFolderMenuItem", "attachSurveyFileMenuItem"],
                    "a Folder outside every cave takes caves, folders and survey files")

            mouseClick(findChild(submenu, "addCaveMenuItem"))
            tryCompare(folder, "childNodeCount", 1, 5000)
            const cave = folder.childNode(0)
            compare(cave.kind, SurveyNodeKind.Cave)
            compare(cave.name, "New Cave")

            //The new cave's row is opened up to and its name is ready to type.
            commitNewName("Side Cave")
            tryCompare(cave, "name", "Side Cave", 5000)
            tryCompare(tree, "rows", 2, 5000)
            tryCompare(tree, "currentRow", 1, 5000)
            const caveIcon = findChild(findChild(page, "caveDelegate1"), "kindIcon")
            verify(caveIcon !== null && caveIcon.visible, "a Cave row carries its kind icon")
            verify(String(caveIcon.source).indexOf("caveKind.svg") >= 0, "a Cave draws the cave icon")
        }

        function test_insideACaveTheMenuOffersTripsAndSections() {
            const cave = addCave("Side Cave", 1)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            let submenu = openAddSubmenu(openRowMenu(page, tree, 0))
            compare(menuEntryNames(submenu),
                    ["addTripMenuItem", "addSectionMenuItem", "attachSurveyFileMenuItem"],
                    "a Cave takes trips, sections and survey files, never a cave")
            compare(submenu.itemAt(1).text, "Add Section")

            mouseClick(findChild(submenu, "addSectionMenuItem"))
            tryCompare(cave, "childNodeCount", 1, 5000)
            const section = cave.childNode(0)
            compare(section.name, "New Section")
            compare(section.kind, SurveyNodeKind.Folder, "a Section is a Folder")
            verify(!section.takesCaves, "a Folder inside a cave takes trips")

            commitNewName("Upper level")
            tryCompare(section, "name", "Upper level", 5000)

            //The Section's row sits under the cave's, ahead of the cave's trip.
            tryCompare(tree, "rows", 3, 5000)
            tryCompare(tree, "currentRow", 1, 5000)
            const sectionChip = findChild(findChild(page, "caveKind1"), "kindChip")
            compare(sectionChip.text, "Section", "a Folder inside a cave reads Section")
            const sectionIcon = findChild(findChild(page, "caveDelegate1"), "kindIcon")
            verify(sectionIcon !== null && sectionIcon.visible, "a Section row carries its kind icon")
            verify(String(sectionIcon.source).indexOf("signpost-split.svg") >= 0,
                   "a Folder inside a cave draws the signpost, not the folder")

            submenu = openAddSubmenu(openRowMenu(page, tree, 1))
            compare(menuEntryNames(submenu),
                    ["addTripMenuItem", "addSectionMenuItem", "attachSurveyFileMenuItem"],
                    "a Section offers no cave either")

            mouseClick(findChild(submenu, "addTripMenuItem"))
            tryCompare(section, "tripCount", 1, 5000)
            tryCompare(tree, "rows", 4, 5000)
            tryCompare(tree, "currentRow", 2, 5000)
            verify(tree.surveyTree.currentObject === section.trip(0),
                   "the new trip's row is current")
        }

        // A Folder's chip follows where the Folder sits: Section inside a
        // cave, Folder at the top level, relabeled as it moves between them.
        function test_folderChipReadsSectionInsideACave() {
            const cave = RootData.region.addNode(null, SurveyNodeKind.Cave, "Side Cave")
            const section = RootData.region.addNode(cave, SurveyNodeKind.Folder, "Upper level")
            const folder = RootData.region.addNode(null, SurveyNodeKind.Folder, "Kentucky")

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)
            tree.surveyTree.expandAll()
            tryCompare(tree, "rows", 3, 5000)

            // The kind chip text of the row that draws object.
            const chipTextFor = (object) => {
                for (let row = 0; row < tree.rows; row++) {
                    const nameCell = findChild(page, "caveDelegate" + row)
                    if (nameCell !== null && nameCell.object === object) {
                        const kindCell = findChild(page, "caveKind" + row)
                        const chip = kindCell === null ? null : findChild(kindCell, "kindChip")
                        return chip === null ? "" : chip.text
                    }
                }
                return ""
            }
            tryVerify(() => chipTextFor(folder) === "Folder", 5000, "a top-level Folder's chip reads Folder")
            tryVerify(() => chipTextFor(section) === "Section", 5000, "a Folder inside a cave reads Section")

            RootData.region.moveNode(section, null, 0)
            tryVerify(() => section.parentNode !== cave, 5000, "the Section leaves the cave")
            tryVerify(() => chipTextFor(section) === "Folder", 5000,
                      "a Section moved to the top level reads Folder")
        }

        // The chip relabels a native node: label and icon change, the node's
        // trips and length stay.
        function test_kindChipRelabelsOnlyTheChipAndIcon() {
            const cave = addCave("Side Cave", 2)
            addShotToTrip(cave.trip(0))

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            const model = tree.model
            tryVerify(() => model.data(model.index(0, SurveyTreeModel.Stations),
                                       SurveyTreeModel.StationCountRole) === 2,
                      10000, "the cave's trips report their stations")
            tryVerify(() => cave.length.value > 0, 10000, "the cave's length is solved")
            const lengthBefore = cave.length.value

            const chip = findChild(findChild(page, "caveKind0"), "kindChip")
            verify(chip.pickable, "a native node's chip is a picker")
            compare(chip.text, "Cave")
            mouseClick(chip)

            let picker = null
            tryVerify(() => {
                          picker = findChild(chip, "kindPickerMenu")
                          return picker !== null && picker.opened
                      }, 5000, "the chip opens the Cave/Folder picker")
            compare(menuEntryNames(picker), ["kindPickerCave", "kindPickerFolder"])
            compare(picker.itemAt(0).text, "✓ Cave", "the label in force is checked")

            mouseClick(findChild(picker, "kindPickerFolder"))
            tryCompare(cave, "kind", SurveyNodeKind.Folder, 5000)

            tryVerify(() => {
                          const kindCell = findChild(page, "caveKind0")
                          const folderChip = kindCell === null ? null : findChild(kindCell, "kindChip")
                          return folderChip !== null && folderChip.text === "Folder"
                      }, 5000, "the chip reads Folder")
            tryVerify(() => {
                          const icon = findChild(findChild(page, "caveDelegate0"), "kindIcon")
                          return icon !== null && icon.visible
                                  && String(icon.source).indexOf("folder.svg") >= 0
                      }, 5000, "the row draws the folder icon")
            compare(cave.tripCount, 2, "the node keeps its trips")
            compare(cave.length.value, lengthBefore, "the node keeps its length")
            compare(model.data(model.index(0, SurveyTreeModel.Trips), SurveyTreeModel.TripCountRole), 2,
                    "the row still counts its trips")

            const folderChip = findChild(findChild(page, "caveKind0"), "kindChip")
            mouseClick(folderChip)
            tryVerify(() => {
                          picker = findChild(folderChip, "kindPickerMenu")
                          return picker !== null && picker.opened
                      }, 5000, "the picker opens again")
            mouseClick(findChild(picker, "kindPickerCave"))
            tryCompare(cave, "kind", SurveyNodeKind.Cave, 5000)
            compare(cave.tripCount, 2)
        }

        // Delete… on a node counts every node and trip it takes along, and
        // undo brings them back.
        function test_deletingANodeCountsItsNodesAndTrips() {
            RootData.region.addNode(null, SurveyNodeKind.Folder)
            const folder = RootData.region.cave(0)
            folder.name = "Kentucky"
            const cave = RootData.region.addNode(folder, SurveyNodeKind.Cave)
            cave.addTrip()
            const section = RootData.region.addNode(cave, SurveyNodeKind.Folder, "Upper level")
            section.addTrip()

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            mouseClick(findChild(openRowMenu(page, tree, 0), "surveyItemDeleteMenuItem"))

            let askBox = null
            tryVerify(() => {
                          askBox = findChild(page, "removeChallange")
                          return askBox !== null && askBox.visible
                      }, 5000, "Delete… must ask first")
            compare(askBox.message,
                    "Delete <b>Kentucky</b> and its 2 children and 2 trips?")

            mouseClick(findChild(askBox, "removeButton"))
            tryCompare(tree, "rows", 0, 5000)
            compare(RootData.region.caveCount, 0, "the folder is gone with everything in it")

            RootData.undoStack.undo()
            tryCompare(RootData.region, "caveCount", 1, 5000)
            compare(RootData.region.cave(0).name, "Kentucky")
            compare(RootData.region.cave(0).childNode(0).childNode(0).tripCount, 1,
                    "undo restores the section's trip")
        }

        // An empty region points at Add Cave; the first cave retires the
        // hint and deleting the last one brings it back.
        function test_noCavesHintShowsOnlyWhileRegionIsEmpty() {
            let page = gotoDataMainPage()

            const hint = findChild(page, "noCavesHint")
            verify(hint !== null, "the empty state must exist")
            tryVerify(() => hint.visible, 5000, "an empty region explains itself")
            compare(findChild(hint, "helpText").text,
                    "No cave yet — add one here, or use the menu beside this button to add one from a survey file.")

            const addBar = findChild(page, "addCave")
            verify(addBar !== null, "the Add Cave bar must exist")

            // triangleOffset is 0, so the arrow tip is the box's origin.
            tryVerify(() => {
                          const tip = hint.mapToItem(page, 0, 0)
                          const anchor = addBar.mapToItem(page, addBar.width / 2.0, addBar.height)
                          return Math.abs(tip.x - anchor.x) < 2
                                  && Math.abs(tip.y - anchor.y) < 2
                      }, 5000, "the arrow lands on Add Cave")

            const addButton = findChild(addBar, "addButton")
            verify(addButton !== null, "the Add Cave button must exist")
            mouseClick(addButton)
            tryCompare(RootData.region, "caveCount", 1, 5000)

            // Add Cave opens the new cave's page; the hint is read on the
            // Data page so its visibility reflects the cave count alone.
            page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)
            tryVerify(() => !hint.visible, 5000, "the first cave retires the hint")

            focusTree(tree)
            keyClick(Qt.Key_F10, Qt.ShiftModifier)
            const menu = rowContextMenu(page, "caveDelegate0")
            mouseClick(findChild(menu, "surveyItemDeleteMenuItem"))

            let askBox = null
            tryVerify(() => {
                          askBox = findChild(page, "removeChallange")
                          return askBox !== null && askBox.visible
                      }, 5000, "Delete… must ask first")
            mouseClick(findChild(askBox, "removeButton"))

            tryCompare(tree, "rows", 0, 5000)
            tryVerify(() => hint.visible, 5000,
                      "deleting the last cave brings the hint back")
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

        //! The resize grip on the left edge of header cell \a column.
        function resizeGripOf(page, column) {
            const cell = headerCell(page, column)
            const grip = findChild(cell, "surveyTreeResizeGrip" + column)
            verify(grip !== null, "header cell " + column + " has a resize grip")
            verify(grip.visible, "header cell " + column + "'s resize grip shows")
            return grip
        }

        //! Drags header cell \a column's left boundary \a dx pixels sideways,
        //! laid out on the page because the grip follows the boundary it moves.
        function dragColumnBoundary(page, column, dx) {
            const grip = resizeGripOf(page, column)
            const start = grip.mapToItem(page, grip.width / 2, grip.height / 2)
            mouseDrag(page, start.x, start.y, dx, 0)
        }

        //! The Data page over one cave, in a window wide enough that the Name
        //! column has room to give.
        function gotoWideTree() {
            rootId.width = wideWindowWidth
            addCave("Alpha Cave", 1)
            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)
            waitForRendering(page)
            return { page: page, tree: tree }
        }

        // Dragging the Name|Kind boundary leftward widens Kind by the drag,
        // Name gives up the same, the rows' cells follow the header, and the
        // width is stored for the node page's tree too.
        function test_aDraggedHeaderBoundaryResizesItsColumn() {
            const view = gotoWideTree()
            const page = view.page
            const tree = view.tree
            const kindHeader = headerCell(page, SurveyTreeModel.Kind)
            const nameHeader = headerCell(page, SurveyTreeModel.Name)
            const startKind = kindHeader.width
            const startName = nameHeader.width
            const drag = 40
            verify(startName - Theme.treeNameColumnMinimumWidth > drag,
                   "the Name column (" + startName + ") has room to give " + drag)

            verify(findChild(nameHeader, "surveyTreeResizeGrip" + SurveyTreeModel.Name) === null,
                   "the Name column takes the remainder, so it has no grip")

            dragColumnBoundary(page, SurveyTreeModel.Kind, -drag)
            tryVerify(() => Math.abs(kindHeader.width - (startKind + drag)) <= 2, 5000,
                      "the Kind header widens by the drag, from " + startKind
                      + " to about " + (startKind + drag) + ", got " + kindHeader.width)
            const draggedWidth = kindHeader.width
            tryVerify(() => Math.abs(nameHeader.width - (startName - (draggedWidth - startKind))) <= 1, 5000,
                      "the Name header gives up what Kind took, from " + startName
                      + ", got " + nameHeader.width)

            tryVerify(() => {
                          const cell = tree.itemAtCell(Qt.point(SurveyTreeModel.Kind, 0))
                          return cell !== null && cell.width === draggedWidth
                      }, 5000, "the cave row's Kind cell follows its column to " + draggedWidth)
            tryVerify(() => {
                          const cell = tree.itemAtCell(Qt.point(SurveyTreeModel.Name, 0))
                          return cell !== null && cell.width === nameHeader.width
                      }, 5000, "the cave row's Name cell follows its column")

            compare(Number(treeColumnSettingsId.value("kindColumnWidth", 0)), draggedWidth,
                    "the dragged width is stored under the survey tree's settings")

            // The cave's own page hosts the same tree and reads the width back.
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Node=Alpha Cave"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      5000, "the cave's node page should show")
            const nodePage = RootData.pageView.currentPageItem
            let nodeKindHeader = null
            tryVerify(() => {
                          nodeKindHeader = findChild(nodePage, "surveyTreeHeaderCell" + SurveyTreeModel.Kind)
                          return nodeKindHeader !== null
                      }, 5000, "the node page's tree has a Kind header")
            tryCompare(nodeKindHeader, "width", draggedWidth, 5000,
                       "the node page's tree shows the width dragged on the Data page")
        }

        // A drag far rightward stops the column at the minimum column width.
        function test_aTreeColumnNarrowsNoFurtherThanItsMinimum() {
            const view = gotoWideTree()
            verify(Theme.tableColumnMinimumWidth > 0, "Theme names a minimum column width")
            const declHeader = headerCell(view.page, SurveyTreeModel.Decl)
            verify(declHeader.width > Theme.tableColumnMinimumWidth,
                   "the Decl column starts wider than the minimum")

            dragColumnBoundary(view.page, SurveyTreeModel.Decl, 1000)
            tryCompare(declHeader, "width", Theme.tableColumnMinimumWidth, 5000,
                       "the Decl column stops at the minimum width")
        }

        // Widening a column stops where the Name column reaches its own
        // minimum, so the tree still fits the view.
        function test_aTreeColumnWidensOnlyAsFarAsNameGives() {
            const view = gotoWideTree()
            const kindHeader = headerCell(view.page, SurveyTreeModel.Kind)
            const nameHeader = headerCell(view.page, SurveyTreeModel.Name)
            const startKind = kindHeader.width
            const room = nameHeader.width - Theme.treeNameColumnMinimumWidth
            verify(room > 0, "the Name column has room to give")

            dragColumnBoundary(view.page, SurveyTreeModel.Kind, -5000)
            tryVerify(() => Math.abs(kindHeader.width - (startKind + room)) <= 2, 5000,
                      "the Kind column widens from " + startKind + " to the Name column's spare room, about "
                      + (startKind + room) + ", got " + kindHeader.width)
            tryVerify(() => Math.abs(nameHeader.width - Theme.treeNameColumnMinimumWidth) <= 2, 5000,
                      "the Name column stops at its minimum, got " + nameHeader.width)
        }

        // The Data page's tree carries a vertical bar once its expanded rows
        // outgrow it, and the bar covers none of the last column.
        function test_anExpandedLongTreeShowsItsScrollBar() {
            const tripCount = 300
            addCave("Long Cave", tripCount)
            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            const bar = findChild(page, "surveyTreeVerticalScrollBar")
            verify(bar !== null, "the Data page's tree has its own vertical scrollbar")
            tryCompare(bar, "size", 1, 5000, "one collapsed cave leaves the bar no handle")

            tree.expand(0)
            tryCompare(tree, "rows", tripCount + 1, 5000)
            waitForRendering(page)
            tryVerify(() => bar.size < 1, 5000,
                      "the bar's handle shows part of the rows, size " + bar.size)
            const declRight = () => {
                const cell = tree.itemAtCell(Qt.point(SurveyTreeModel.Decl, 0))
                return cell !== null ? cell.mapToItem(tree, cell.width, 0).x : Number.NaN
            }
            tryVerify(() => declRight() <= bar.x + 1, 5000,
                      "the Decl column ends at " + declRight() + ", left of the bar at " + bar.x)
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

            verify(!findChild(cleanBar, "warningBadge").visible,
                   "a node without a warning has no warning badge")
            verify(!findChild(cleanBar, "fatalBadge").visible,
                   "and no fatal badge")
            verify(!cleanBar.visible,
                   "a clean node hides the bar so its name sits flush")

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

        // --- C5.4: Move to… ---

        // Kentucky (Folder) › Side Cave (Cave) › Upper level (Section), with
        // Trip 1 under Side Cave, beside a top-level Other Cave. Fully
        // expanded, the view rows are Kentucky 0, Side Cave 1, Upper level 2,
        // Trip 1 3 and Other Cave 4.
        function setupMoveTree() {
            const kentucky = RootData.region.addNode(null, SurveyNodeKind.Folder, "Kentucky")
            const sideCave = RootData.region.addNode(kentucky, SurveyNodeKind.Cave, "Side Cave")
            const upperLevel = RootData.region.addNode(sideCave, SurveyNodeKind.Folder, "Upper level")
            sideCave.addTrip()
            const trip = sideCave.trip(0)
            trip.name = "Trip 1"
            const otherCave = RootData.region.addNode(null, SurveyNodeKind.Cave, "Other Cave")
            return { kentucky: kentucky, sideCave: sideCave, upperLevel: upperLevel,
                     trip: trip, otherCave: otherCave }
        }

        // Shows every row of the tree setupMoveTree() builds.
        function expandedMoveTree(page) {
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)
            tree.surveyTree.expandAll()
            tryCompare(tree, "rows", 5, 5000)
            return tree
        }

        // The Name cell of view row \a viewRow once it draws \a object.
        function rowFor(page, prefix, viewRow, object) {
            let row = null
            tryVerify(() => {
                          row = findChild(page, prefix + viewRow)
                          return row !== null && row.object === object
                      }, 5000, prefix + viewRow + " must draw its object")
            return row
        }

        // Opens the context menu of view row \a viewRow, a node row for the
        // "caveDelegate" prefix and a trip row for "tripDelegate".
        function openMenuOn(page, tree, viewRow, prefix) {
            tree.surveyTree.setCurrentRow(viewRow)
            tree.forceActiveFocus()
            tryVerify(() => tree.activeFocus && tree.currentRow === viewRow, 5000,
                      "row " + viewRow + " must be current")
            keyClick(Qt.Key_F10, Qt.ShiftModifier)
            return rowContextMenu(page, prefix + viewRow)
        }

        function moveBanner(page) {
            const banner = findChild(page, "surveyMoveBanner")
            verify(banner !== null, "the Data page carries the move banner")
            return banner
        }

        // A click where the row's name ends: past the caret, on the cell that
        // takes an armed move's click.
        function clickRow(row) {
            mouseClick(row, row.width - Theme.delegatePadding, row.height / 2)
        }

        // Move to… arms the tree: the banner asks, every row that takes the
        // move is a target and every other row says why it is not; a click on
        // a target lands the move, and the breadcrumb follows the trip.
        function test_moveToArmsTheTreeAndAClickOnATargetLandsIt() {
            const t = setupMoveTree()
            const page = gotoDataMainPage()
            const tree = expandedMoveTree(page)
            const banner = moveBanner(page)
            verify(!banner.visible, "no banner before a move is armed")

            const menu = openMenuOn(page, tree, 3, "tripDelegate")
            const moveItem = findChild(menu, "surveyItemMoveMenuItem")
            verify(moveItem !== null && moveItem.visible, "a native trip offers Move to…")
            compare(moveItem.text, "Move to…")
            mouseClick(moveItem)

            tryVerify(() => banner.visible, 5000, "Move to… shows the banner")
            compare(findChild(banner, "surveyMoveBannerLabel").text,
                    "Click where to move Trip 1 — Esc cancels")
            verify(!findChild(banner, "surveyMoveTopLevelButton").visible,
                   "the top level takes no trip")

            const upperRow = rowFor(page, "caveDelegate", 2, t.upperLevel)
            tryVerify(() => upperRow.isMoveTarget, 5000, "a Section takes a trip")
            verify(rowFor(page, "caveDelegate", 4, t.otherCave).isMoveTarget, "another cave takes a trip")

            const kentuckyRow = rowFor(page, "caveDelegate", 0, t.kentucky)
            verify(!kentuckyRow.isMoveTarget)
            compare(kentuckyRow.moveReason, "Trips go in a cave or a section")
            compare(rowFor(page, "caveDelegate", 1, t.sideCave).moveReason, "Already here")
            compare(rowFor(page, "tripDelegate", 3, t.trip).moveReason, "This is what's moving")

            //A muted row takes no click.
            clickRow(kentuckyRow)
            verify(RegionSurveyTree.moveActive, "a click on a muted row leaves the move armed")
            compare(t.trip.parentNode, t.sideCave)

            clickRow(upperRow)
            tryCompare(t.trip, "parentNode", t.upperLevel, 5000)
            tryVerify(() => !banner.visible, 5000, "landing the move retires the banner")
            compare(t.sideCave.tripCount, 0)
            compare(t.upperLevel.tripCount, 1)

            tree.surveyTree.openObject(t.trip)
            tryCompare(RootData.pageSelectionModel, "currentPageAddress",
                       "Source/Data/Node=Kentucky/Node=Side Cave/Node=Upper level/Trip=Trip 1", 5000)

            RootData.undoStack.undo()
            tryCompare(t.trip, "parentNode", t.sideCave, 5000)
        }

        // Esc, leaving the page, and the banner's Cancel each disarm the move.
        function test_escapeLeavingAndCancelDisarmTheMove() {
            const t = setupMoveTree()
            let page = gotoDataMainPage()
            const tree = expandedMoveTree(page)
            const banner = moveBanner(page)

            mouseClick(findChild(openMenuOn(page, tree, 2, "caveDelegate"), "surveyItemMoveMenuItem"))
            tryVerify(() => banner.visible, 5000, "Move to… shows the banner")
            verify(findChild(banner, "surveyMoveTopLevelButton").visible,
                   "the top level takes a section")
            keyClick(Qt.Key_Escape)
            tryVerify(() => !RegionSurveyTree.moveActive, 5000, "Esc cancels the move")
            verify(!banner.visible)

            RegionSurveyTree.startMove(RegionSurveyTree.indexOf(t.upperLevel))
            tryVerify(() => banner.visible, 5000)
            mouseClick(findChild(banner, "surveyMoveCancelButton"))
            tryVerify(() => !RegionSurveyTree.moveActive, 5000, "Cancel cancels the move")

            RegionSurveyTree.startMove(RegionSurveyTree.indexOf(t.upperLevel))
            verify(RegionSurveyTree.moveActive)
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => !RegionSurveyTree.moveActive, 5000, "leaving the Data page cancels the move")
            compare(t.upperLevel.parentNode, t.sideCave, "nothing moved")
        }

        // A move that ties stations asks first and names the count; Move
        // lands it with the tie, and one undo takes both back.
        function test_aMoveThatTiesStationsAsksFirst() {
            const t = setupMoveTree()
            t.sideCave.addTrip()
            const partner = t.sideCave.trip(1)
            partner.name = "Trip 2"
            const stationNames = [[t.trip, "a1", "a2"], [partner, "a2", "b1"]]
            for(let i = 0; i < stationNames.length; i++) {
                const trip = stationNames[i][0]
                trip.addNewChunk()
                const chunk = trip.chunk(0)
                chunk.setData(SurveyChunk.StationNameRole, 0, stationNames[i][1])
                chunk.setData(SurveyChunk.StationNameRole, 1, stationNames[i][2])
                chunk.setData(SurveyChunk.ShotDistanceRole, 0, "10")
                chunk.setData(SurveyChunk.ShotCompassRole, 0, "0")
                chunk.setData(SurveyChunk.ShotClinoRole, 0, "0")
            }

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)
            tree.surveyTree.expandAll()
            tryCompare(tree, "rows", 6, 5000)
            const banner = moveBanner(page)

            RegionSurveyTree.startMove(RegionSurveyTree.indexOf(t.trip))
            tryVerify(() => banner.visible, 5000)

            clickRow(rowFor(page, "caveDelegate", 2, t.upperLevel))
            tryCompare(findChild(banner, "surveyMoveBannerLabel"), "text",
                       "Move Trip 1 to Upper level? 1 station shared with Side Cave becomes a tie.", 5000)
            compare(t.trip.parentNode, t.sideCave, "the question comes before the move")
            //The Move button appears with the question, so the banner is laid
            //out again before it is clicked.
            waitForRendering(banner)

            const equatesBefore = RootData.region.equates.count
            mouseClick(findChild(banner, "surveyMoveConfirmButton"))
            tryCompare(t.trip, "parentNode", t.upperLevel, 5000)
            compare(RootData.region.equates.count, equatesBefore + 1, "the shared station became a tie")
            tryVerify(() => !banner.visible, 5000)

            RootData.undoStack.undo()
            tryCompare(t.trip, "parentNode", t.sideCave, 5000)
            compare(RootData.region.equates.count, equatesBefore, "one undo takes the tie back too")
        }

        // --- C5.5: Move to… follow-ups ---

        // The view row drawing \a object, -1 while no row of the view does.
        function viewRowOf(tree, object) {
            for (let row = 0; row < tree.rows; row++) {
                if (tree.surveyTree.objectAtRow(row) === object) {
                    return row
                }
            }
            return -1
        }

        // True while \a object has a row of the view and that row is current.
        function isShownAndCurrent(tree, object) {
            return viewRowOf(tree, object) >= 0 && tree.surveyTree.currentObject === object
        }

        // The armed row reads as lifted: full opacity and its own frame,
        // distinct from the targets' frame and from the muted rows.
        function test_theArmedSubjectReadsAsLifted() {
            const t = setupMoveTree()
            const page = gotoDataMainPage()
            const tree = expandedMoveTree(page)

            mouseClick(findChild(openMenuOn(page, tree, 3, "tripDelegate"), "surveyItemMoveMenuItem"))
            tryVerify(() => moveBanner(page).visible, 5000, "Move to… shows the banner")

            const subjectRow = rowFor(page, "tripDelegate", 3, t.trip)
            tryVerify(() => subjectRow.isMoveSource, 5000, "the moving row is the move's source")
            compare(findChild(subjectRow, "rowContent").opacity, 1.0, "the moving row keeps full opacity")
            verify(findChild(subjectRow, "moveSourceOutline").visible, "the moving row draws its own frame")

            const mutedRow = rowFor(page, "caveDelegate", 0, t.kentucky)
            verify(!mutedRow.isMoveSource && !mutedRow.isMoveTarget)
            compare(findChild(mutedRow, "rowContent").opacity, Theme.disabledOpacity,
                    "a row that cannot take the move steps back")
            verify(!findChild(mutedRow, "moveSourceOutline").visible)

            const targetRow = rowFor(page, "caveDelegate", 2, t.upperLevel)
            verify(targetRow.isMoveTarget)
            verify(!findChild(targetRow, "moveSourceOutline").visible, "a target draws the targets' frame only")

            RegionSurveyTree.cancelMove()
            tryVerify(() => !findChild(subjectRow, "moveSourceOutline").visible, 5000,
                      "disarming drops the frame")
        }

        // A trip moved into a collapsed Section opens that Section and is the
        // current row afterward, and undo and redo each show it where they
        // put it.
        function test_aMovedTripIsShownAndCurrentWhereItLands() {
            const t = setupMoveTree()
            t.upperLevel.addTrip()
            t.upperLevel.trip(0).name = "Upper survey"

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)
            tree.surveyTree.expandAll()
            tryCompare(tree, "rows", 6, 5000)
            const upperViewRow = viewRowOf(tree, t.upperLevel)
            tree.collapse(upperViewRow)
            tryCompare(tree, "rows", 5, 5000)

            RegionSurveyTree.startMove(RegionSurveyTree.indexOf(t.trip))
            tryVerify(() => moveBanner(page).visible, 5000)
            clickRow(rowFor(page, "caveDelegate", upperViewRow, t.upperLevel))

            tryCompare(t.trip, "parentNode", t.upperLevel, 5000)
            tryVerify(() => isShownAndCurrent(tree, t.trip), 5000,
                      "the moved trip is visible and current in the Section it landed in")
            verify(tree.isExpanded(viewRowOf(tree, t.upperLevel)), "the Section opened to show it")

            //Folded away again, so undo has to open Side Cave to show it.
            tree.collapse(viewRowOf(tree, t.sideCave))
            tryVerify(() => viewRowOf(tree, t.trip) < 0, 5000)
            RootData.undoStack.undo()
            tryCompare(t.trip, "parentNode", t.sideCave, 5000)
            tryVerify(() => isShownAndCurrent(tree, t.trip), 5000,
                      "undo shows the trip back in Side Cave")

            tree.collapse(viewRowOf(tree, t.upperLevel))
            RootData.undoStack.redo()
            tryCompare(t.trip, "parentNode", t.upperLevel, 5000)
            tryVerify(() => isShownAndCurrent(tree, t.trip), 5000,
                      "redo shows the trip in the Section again")
        }

        // Renaming the armed row in place renames it in the banner too.
        function test_renamingTheArmedRowUpdatesTheBanner() {
            const t = setupMoveTree()
            const page = gotoDataMainPage()
            const tree = expandedMoveTree(page)
            const banner = moveBanner(page)

            RegionSurveyTree.startMove(RegionSurveyTree.indexOf(t.upperLevel))
            tryVerify(() => banner.visible, 5000)
            const label = findChild(banner, "surveyMoveBannerLabel")
            compare(label.text, "Click where to move Upper level — Esc cancels")

            const upperRow = rowFor(page, "caveDelegate", 2, t.upperLevel)
            upperRow.startRename()
            commitNewName("Lower level")
            tryCompare(t.upperLevel, "name", "Lower level", 5000)
            tryCompare(label, "text", "Click where to move Lower level — Esc cancels", 5000)
            verify(RegionSurveyTree.moveActive, "renaming leaves the move armed")
        }

        // Move to… is a row verb only: the node page's header carries no
        // button for it.
        function test_theNodePageShowsNoMoveButton() {
            const t = setupMoveTree()
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Node=Kentucky/Node=Side Cave"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage"
                            && RootData.pageView.currentPageItem.currentNode === t.sideCave,
                      5000, "the Side Cave page must show")
            const nodePage = RootData.pageView.currentPageItem
            waitForRendering(nodePage)
            const nameText = findChild(nodePage, "nodeNameText")
            verify(nameText !== null, "the header is drawn")
            compare(findChild(nodePage, "moveNodeButton"), null, "the node page shows no Move to… button")
            const headerItems = nameText.parent.children
            for (let i = 0; i < headerItems.length; ++i) {
                verify(headerItems[i].text !== "Move to…",
                       "no header control reads Move to…")
            }
        }

        // --- C7.4a: sourced rows, gray books, and the sourced Delete… texts ---

        // Saves the project, attaches walls_project under the region's root and
        // adds a native trip to its Surveys book. Returns the source root.
        function attachWallsProject() {
            RootData.account.name = "Sourced Test"
            RootData.account.email = "sourced.test@example.com"
            const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tmpPath + "/sourcedTree.cwproj"), "saveAs should succeed")
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            const regionRoot = RootData.region.rootNode
            RootData.attachSource(regionRoot,
                                  TestHelper.testcasesDatasetSourcePath("external-centerlines/walls_project/KAUA.WPJ"))
            tryVerify(() => regionRoot.childNodeCount === 1
                            && regionRoot.childNode(0).externalCenterline.entryFile !== ""
                            && regionRoot.childNode(0).childNodeCount === 3,
                      10000, "the attach must build the subtree")
            RootData.futureManagerModel.waitForFinished()

            const surveys = regionRoot.childNode(0).childNode(2)
            compare(surveys.name, "Surveys")
            surveys.addTrip()
            surveys.trip(surveys.tripCount - 1).name = "Native trip"
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            return regionRoot.childNode(0)
        }

        // attachWallsProject(), then the project opened again from disk.
        // Returns the reloaded source root.
        function attachWallsProjectAndReload() {
            attachWallsProject()
            TestHelper.loadProjectFromPath(RootData.project, RootData.project.filename)
            tryVerify(() => RootData.region.rootNode.childNodeCount === 1
                            && RootData.region.rootNode.childNode(0).childNodeCount === 3,
                      10000, "the project must load its subtree")
            RootData.futureManagerModel.waitForFinished()
            return RootData.region.rootNode.childNode(0)
        }

        // The child of \a node named \a name.
        function childNamed(node, name) {
            for (let i = 0; i < node.childNodeCount; i++) {
                if (node.childNode(i).name === name) {
                    return node.childNode(i)
                }
            }
            return null
        }

        // The Delete… prompt's message for the row at \a viewRow, closed again
        // without removing anything.
        function deletePromptFor(page, tree, viewRow, prefix) {
            const menu = openMenuOn(page, tree, viewRow, prefix)
            // Triggered rather than clicked: on the cached Data page, after a
            // project is opened again, a synthetic click on the menu reaches
            // the item's press and never its trigger offscreen.
            // test_sourcedDeleteClickAsksFirst covers the click itself.
            findChild(menu, "surveyItemDeleteMenuItem").triggered()
            let askBox = null
            tryVerify(() => {
                          askBox = findChild(page, "removeChallange")
                          return askBox !== null && askBox.visible
                      }, 5000, "Delete… on " + prefix + viewRow + " must ask first")
            return askBox
        }

        // A source root opens one level as the project loads, its books with
        // no survey data draw gray, and a sourced node offers no rename.
        function test_sourcedRowsOpenOneLevelOnLoadAndGrayTheEmptyBooks() {
            const root = attachWallsProjectAndReload()
            const page = gotoDataMainPage()
            const tree = surveyTree(page)

            tryCompare(tree, "rows", 4, 10000)
            verify(tree.isExpanded(0), "the source root opens one level on load")
            verify(!tree.isExpanded(viewRowOf(tree, childNamed(root, "Surveys"))),
                   "the levels below stay closed")

            const drawingsRow = rowFor(page, "caveDelegate", viewRowOf(tree, childNamed(root, "Drawings")),
                                       childNamed(root, "Drawings"))
            verify(drawingsRow.grayed, "a book with no survey data draws gray")
            compare(drawingsRow.textColor, Theme.textSubtle)
            const fixedRow = rowFor(page, "caveDelegate", viewRowOf(tree, childNamed(root, "Fixed Points")),
                                    childNamed(root, "Fixed Points"))
            verify(fixedRow.grayed, "a book holding only a fix draws gray")
            const surveysRow = rowFor(page, "caveDelegate", viewRowOf(tree, childNamed(root, "Surveys")),
                                      childNamed(root, "Surveys"))
            verify(!surveysRow.grayed, "a book with surveys under it reads normally")
            verify(!rowFor(page, "caveDelegate", 0, root).grayed, "the root reads normally")

            // The kind chip names the file the node's shape comes from.
            const chip = findChild(findChild(page, "caveKind" + viewRowOf(tree, childNamed(root, "Surveys"))),
                                   "kindChip")
            verify(chip.sourced, "a derived node's chip is the sourced one")
            compare(chip.sourceFileName, "KAUA.WPJ")

            // F2 renames a native row in place; a sourced row keeps its file's name.
            verify(!surveysRow.canRename, "a sourced node offers no rename")
            tree.surveyTree.setCurrentRow(surveysRow.row)
            tree.forceActiveFocus()
            tryVerify(() => tree.activeFocus && tree.currentRow === surveysRow.row, 5000)
            keyClick(Qt.Key_F2)
            verify(!surveysRow.renaming, "F2 opens no editor on a sourced node")

            const rootMenu = openMenuOn(page, tree, 0, "caveDelegate")
            verify(findChild(rootMenu, "surveyItemRenameMenuItem") === null, "no Rename on the root")
            const reload = findChild(rootMenu, "surveyItemReloadMenuItem")
            verify(reload !== null, "the root's menu offers Reload")
            tryVerify(() => reload.enabled, 5000, "Reload is offered for a file still on disk")
            verify(findChild(rootMenu, "surveyItemReplaceMenuItem") !== null,
                   "the root's menu offers Replace…")
            rootMenu.close()
            tryVerify(() => !rootMenu.visible, 5000)

            const bookMenu = openMenuOn(page, tree, surveysRow.row, "caveDelegate")
            verify(findChild(bookMenu, "surveyItemReloadMenuItem") === null,
                   "Reload lives on the root alone")
            bookMenu.close()
            tryVerify(() => !bookMenu.visible, 5000)
        }

        // Sourced data is all or nothing: the source root deletes with its
        // copied file, while the nodes and trips its file derives offer no
        // Delete…. A native trip filed under a derived node keeps its own.
        function test_onlyTheSourceRootAndNativeRowsOfferDelete() {
            const root = attachWallsProjectAndReload()
            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 4, 10000)
            tree.surveyTree.expandAll()
            tryVerify(() => tree.rows === 9, 5000, "every row of the subtree shows")

            const askBox = deletePromptFor(page, tree, 0, "caveDelegate")
            compare(askBox.message,
                    "Delete <b>KAUA</b>? Removes 4 books, 3 derived trips and 1 native trip. "
                    + "The copied KAUA.WPJ is removed from the project.")
            mouseClick(findChild(askBox, "cancelButton"))
            tryVerify(() => !askBox.visible, 5000)

            const rootMenu = openMenuOn(page, tree, 0, "caveDelegate")
            const rootNames = menuEntryNames(rootMenu)
            const reloadAt = rootNames.indexOf("surveyItemReloadMenuItem")
            const replaceAt = rootNames.indexOf("surveyItemReplaceMenuItem")
            const deleteAt = rootNames.indexOf("surveyItemDeleteMenuItem")
            verify(reloadAt >= 0 && reloadAt < replaceAt && replaceAt < deleteAt,
                   "the root's menu runs Reload, Replace…, Delete…; got: " + rootNames)
            rootMenu.close()
            tryVerify(() => !rootMenu.visible, 5000)

            const surveys = childNamed(root, "Surveys")
            const surveysMenu = openMenuOn(page, tree, viewRowOf(tree, surveys), "caveDelegate")
            compare(findChild(surveysMenu, "surveyItemDeleteMenuItem"), null,
                    "a derived node offers no Delete…")
            surveysMenu.close()
            tryVerify(() => !surveysMenu.visible, 5000)

            const dec2001 = childNamed(surveys, "Dec 2001 Surveys")
            const derivedTrip = dec2001.trip(0)
            compare(derivedTrip.name, "D Survey 1")
            const derivedTripMenu = openMenuOn(page, tree, viewRowOf(tree, derivedTrip), "tripDelegate")
            compare(findChild(derivedTripMenu, "surveyItemDeleteMenuItem"), null,
                    "a derived trip offers no Delete…")
            derivedTripMenu.close()
            tryVerify(() => !derivedTripMenu.visible, 5000)

            let nativeTrip = null
            for (let i = 0; i < surveys.tripCount; i++) {
                if (surveys.trip(i).name === "Native trip") {
                    nativeTrip = surveys.trip(i)
                }
            }
            verify(nativeTrip !== null, "Surveys holds the native trip")
            const nativeMenu = openMenuOn(page, tree, viewRowOf(tree, nativeTrip), "tripDelegate")
            verify(findChild(nativeMenu, "surveyItemDeleteMenuItem") !== null,
                   "a native trip under a derived node keeps Delete…")
            nativeMenu.close()
            tryVerify(() => !nativeMenu.visible, 5000)
        }

        // A trip with a file of its own leaves with that file, so its row
        // keeps Delete….
        function test_anAttachedTripOffersDelete() {
            RootData.account.name = "Attached Delete Test"
            RootData.account.email = "attached.delete@example.com"
            const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tmpPath + "/attachedDelete.cwproj"), "saveAs should succeed")
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            const cave = addCave("Alpha Cave", 1)
            const trip = cave.trip(0)
            const model = RootData.externalCenterlineManager.attachedCenterlinesModel
            const rowsBefore = model.rowCount()
            RootData.attachTripCenterline(trip,
                                          TestHelper.testcasesDatasetPath("external-centerlines/survex_simple.svx"))
            tryVerify(() => model.rowCount() > rowsBefore, 10000, "the attach must land")
            RootData.futureManagerModel.waitForFinished()
            verify(trip.externalCenterline.entryFile.length > 0, "the trip owns its file")
            compare(trip.sourcePath, "", "an attached trip derives from no other file")

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)
            tree.expand(0)
            tryCompare(tree, "rows", 2, 5000)
            const menu = openMenuOn(page, tree, viewRowOf(tree, trip), "tripDelegate")
            verify(findChild(menu, "surveyItemDeleteMenuItem") !== null,
                   "an attached trip keeps Delete…")
            menu.close()
            tryVerify(() => !menu.visible, 5000)
        }

        // Clicking Delete… on a freshly attached source root asks first.
        function test_sourcedDeleteClickAsksFirst() {
            attachWallsProject()
            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryVerify(() => tree.rows >= 1, 10000)
            const menu = openMenuOn(page, tree, 0, "caveDelegate")
            mouseClick(findChild(menu, "surveyItemDeleteMenuItem"))
            let askBox = null
            tryVerify(() => {
                          askBox = findChild(page, "removeChallange")
                          return askBox !== null && askBox.visible
                      }, 5000, "a clicked Delete… must ask first")
            verify(askBox.message.startsWith("Delete <b>KAUA</b>? Removes "))
            mouseClick(findChild(askBox, "cancelButton"))
            tryVerify(() => !askBox.visible, 5000)
        }

        // Replace… on a source root asks first and counts the scraps on its
        // derived trips that the picked file would strand (§6.11 case 5).
        function test_replaceCountsTheScrapsThatLoseAnchors() {
            const root = attachWallsProject()
            const dec2001 = childNamed(childNamed(root, "Surveys"), "Dec 2001 Surveys")
            const note = TestHelper.addNoteWithScrap(dec2001.trip(0), "Sketch")
            verify(TestHelper.addScrapStation(note, 0, "D2", Qt.point(0.5, 0.5)),
                   "the scrap anchors on one of the file's stations")

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            const menu = openMenuOn(page, tree, 0, "caveDelegate")
            mouseClick(findChild(menu, "surveyItemReplaceMenuItem"))

            let dialog = null
            tryVerify(() => {
                          dialog = findChild(page, "replaceCenterlineDialog")
                          return dialog !== null && findChild(dialog, "sourcePathField").visible
                      }, 5000, "Replace… opens its dialog")
            compare(dialog.owner, root, "the dialog replaces the root's file")

            const anchorLoss = findChild(dialog, "replaceAnchorLossText")
            verify(!anchorLoss.visible, "nothing is stranded before a file is picked")

            // The same file names D2, so nothing is stranded.
            findChild(dialog, "sourcePathField").text =
                    TestHelper.testcasesDatasetSourcePath("external-centerlines/walls_project/KAUA.WPJ")
            tryVerify(() => findChild(dialog, "replaceConfirmButton").enabled, 10000, "the scan lands")
            verify(!anchorLoss.visible, "a file that names every station strands nothing")

            // An unrelated file names none of them.
            findChild(dialog, "sourcePathField").text =
                    TestHelper.testcasesDatasetSourcePath("external-centerlines/survex_simple.svx")
            tryVerify(() => anchorLoss.visible, 10000, "the confirm counts what Replace strands")
            compare(anchorLoss.text,
                    "Replacing strands 1 scrap on 1 note: they sit on stations the new file does not name.")

            mouseClick(findChild(dialog, "replaceCancelButton"))
            tryVerify(() => findChild(page, "replaceCenterlineDialog") === null
                            || !findChild(findChild(page, "replaceCenterlineDialog"), "sourcePathField").visible,
                      5000, "Cancel closes the dialog")
        }

        // Survey data an attached file places stays where the file puts it:
        // its rows offer no Move to….
        function test_attachedRowsOfferNoMove() {
            RootData.account.name = "Move Test"
            RootData.account.email = "move.test@example.com"
            const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tmpPath + "/moveTest.cwproj"), "saveAs should succeed")
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            const model = RootData.externalCenterlineManager.attachedCenterlinesModel
            const rowsBefore = model.rowCount()
            RootData.attachSource(RootData.region.rootNode,
                                  TestHelper.testcasesDatasetPath("external-centerlines/survex_simple.svx"))
            tryVerify(() => model.rowCount() > rowsBefore, 10000, "the attach must land")
            RootData.futureManagerModel.waitForFinished()
            const cave = RootData.region.cave(0)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            const menu = openMenuOn(page, tree, 0, "caveDelegate")
            verify(findChild(menu, "surveyItemMoveMenuItem") === null,
                   "a source root offers no Move to…")
            menu.close()

            // survex_simple's one block is a leaf, so it lists as a derived
            // trip of the file's root.
            tryVerify(() => cave.tripCount > 0, 10000,
                      "the attached file's block is a derived trip")
            tree.surveyTree.expandAll()
            tryVerify(() => tree.rows > 1, 5000)
            const tripMenu = openMenuOn(page, tree, 1, "tripDelegate")
            verify(findChild(tripMenu, "surveyItemMoveMenuItem") === null,
                   "a derived trip offers no Move to…")
            tripMenu.close()
        }

        // --- MP.5: attached rows count the solved stations of their subtree ---

        // Attaches survex_station_counts/counts.svx under the region's root and
        // waits for its blocks to land. Returns the source root.
        function attachStationCountsFile() {
            RootData.account.name = "Counts Test"
            RootData.account.email = "counts.test@example.com"
            const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tmpPath + "/stationCounts.cwproj"), "saveAs should succeed")
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            const regionRoot = RootData.region.rootNode
            RootData.attachSource(regionRoot,
                                  TestHelper.testcasesDatasetSourcePath("external-centerlines/survex_station_counts/counts.svx"))
            tryVerify(() => {
                          const root = regionRoot.childNodeCount === 1 ? regionRoot.childNode(0) : null
                          const system = root !== null ? childNamed(root, "system") : null
                          return system !== null && childNamed(system, "lower") !== null
                      }, 10000, "the attach must build the subtree")
            RootData.futureManagerModel.waitForFinished()
            return regionRoot.childNode(0)
        }

        // The trip named \a name under \a node.
        function tripNamed(node, name) {
            for (let i = 0; i < node.tripCount; i++) {
                if (node.trip(i).name === name) {
                    return node.trip(i)
                }
            }
            return null
        }

        // The file root, a node in the middle, and a leaf trip each read the
        // stations solved under them, where they all read 0 or nothing before.
        function test_attachedRowsCountTheirSolvedStations() {
            // The page is up before the attach, the way the attach dialog runs
            // from it, so its rows take in the counts as the solve lands.
            const page = gotoDataMainPage()
            const root = attachStationCountsFile()
            const system = childNamed(root, "system")
            const lower = childNamed(system, "lower")
            const left = tripNamed(lower, "left")
            verify(left !== null, "lower holds the leaf trip left")

            const tree = surveyTree(page)
            tree.surveyTree.expandAll()
            tryVerify(() => viewRowOf(tree, left) >= 0, 10000, "every row is shown")

            // s1-s3, u1-u4, l1-l3 and r1-r2; equated names count apart.
            tryCellText(page, "caveStations" + viewRowOf(tree, root), "value", "12",
                        "the source root counts every solved station of its file")
            tryCellText(page, "caveStations" + viewRowOf(tree, lower), "value", "5",
                        "a middle node counts the solved stations of its blocks")
            tryCellText(page, "tripStations" + viewRowOf(tree, left), "value", "3",
                        "an attached trip counts its own stations")
        }

        // --- MT.1: a placeholder count until the first solve lands ---

        // Waits for a cell's value to read the placeholder dash, then checks
        // it draws in the subtle text color the page gives an unknown value.
        function tryCellPlaceholder(page, cellObjectName, message) {
            const value = cellValue(page, cellObjectName)
            tryCompare(value, "text", "\u2014", 10000, message + ": " + cellObjectName + "->value")
            verify(Qt.colorEqual(value.color, Theme.textSubtle),
                   message + ": the placeholder draws in Theme.textSubtle, not " + value.color)
        }

        // A file cavern never solves leaves its row, and the native cave it
        // was attached under, on the placeholder: neither shows a partial sum.
        function test_unsolvedAttachedRowsShowThePlaceholderCount() {
            RootData.account.name = "Placeholder Test"
            RootData.account.email = "placeholder.test@example.com"
            const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tmpPath + "/placeholderCount.cwproj"), "saveAs should succeed")
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            const page = gotoDataMainPage()
            const host = addCave("Host Cave", 0)
            RootData.attachSource(host,
                                  TestHelper.testcasesDatasetSourcePath(
                                      "external-centerlines/survex_noisy_failure/noisy_failure.svx"))
            tryVerify(() => host.childNodeCount === 1 && host.childNode(0).tripCount > 0, 10000,
                      "the attach must build the source root and its trip")
            RootData.futureManagerModel.waitForFinished()
            const root = host.childNode(0)

            const tree = surveyTree(page)
            tree.surveyTree.expandAll()
            tryVerify(() => viewRowOf(tree, root) >= 0, 10000, "the source root's row is shown")

            tryCellPlaceholder(page, "caveStations" + viewRowOf(tree, root),
                               "an unsolved source root shows the placeholder")
            tryCellPlaceholder(page, "caveStations" + viewRowOf(tree, host),
                               "a native cave over an unsolved source shows the placeholder")
        }

        // --- MP.6: attached trips report their solved length ---

        // An attached trip owns no shots, so its row reads the length of the
        // legs the solve drew for it.
        function test_attachedTripRowsShowTheirSolvedLength() {
            const page = gotoDataMainPage()
            const root = attachStationCountsFile()
            const system = childNamed(root, "system")
            const lower = childNamed(system, "lower")
            const upper = tripNamed(system, "upper")
            const left = tripNamed(lower, "left")
            verify(upper !== null, "system holds the trip upper")
            verify(left !== null, "lower holds the leaf trip left")

            const tree = surveyTree(page)
            tree.surveyTree.expandAll()
            tryVerify(() => viewRowOf(tree, left) >= 0, 10000, "every row is shown")

            // upper is three 5 m legs; left is two 4 m legs.
            tryCellText(page, "tripLength" + viewRowOf(tree, upper), "value", "15",
                        "an attached trip reads the length of its solved legs")
            tryCellText(page, "tripLength" + viewRowOf(tree, left), "value", "8",
                        "a leaf attached trip reads the length of its solved legs")
        }
    }
}
