import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// The Splays column in the survey table: a station that carries splays shows a
// chip with their count, and clicking it opens read-only splay rows under the
// station. The expansion lives in the model, so this test drives it the way a
// caver does — through the chip — and reads the rows back out of the view.
MainWindowTest {
    id: rootId

    SurveyTableTestHelper {
        id: surveyTableId
        mainWindow: rootId.mainWindow
    }

    TestCase {
        name: "SplayRows"
        when: windowShown

        // The same readings the C++ splay tests use, read out of the one fixture
        // that owns them.
        readonly property var a4Splays: TestHelper.a4SplayReadings()

        function init() {
            surveyTableId.resetToViewPage(this)
        }

        function cleanup() {
            surveyTableId.leaveSurveyTable()
        }

        // The survey table with A1 to A2, and the three a4 splays on A1.
        function gotoSurveyTable() {
            const context = surveyTableId.openSurveyTable(this, "SplayCave", "SplayTrip")

            surveyTableId.setStationName(this, context, 0, "A1")
            surveyTableId.setStationName(this, context, 1, "A2")

            for (let i = 0; i < a4Splays.length; i++) {
                const splay = a4Splays[i]
                TestHelper.addStationSplay(context.chunk, 0,
                                           splay.distance, splay.compass, splay.clino)
            }

            return context
        }

        // Focusing any cell in a chunk brings that chunk's trailing blank
        // station and shot rows into the view, and the Splays cell is a cell
        // like any other — so a click on it costs these two rows on top of the
        // cluster it opens.
        readonly property int virtualRows: 2

        // Every open cluster carries the blank row its next splay is typed
        // into, so opening one costs a row more than the splays it holds.
        readonly property int blankRow: 1

        // Long enough for a hover or a click to have taken effect, for the
        // checks that something stayed away
        readonly property int settleMilliseconds: 100

        function splaysCell(context, indexInChunk) {
            const item = surveyTableId.rowItem(
                           this, context, surveyTableId.stationRow(context, indexInChunk))

            let cell = null
            tryVerify(() => {
                cell = findChild(item, "splaysBox." + surveyTableId.stationRow(context, indexInChunk))
                return cell !== null
            }, 5000, "station " + indexInChunk + " should have a splays cell")
            return cell
        }

        // A reading cell of any row, by the cell it stands for — a splay row's
        // SurveyEditorCellIndex.SplayDistanceCell and friends, or a shot row's
        // ShotDistanceCell.
        function cellBox(context, row, cellRole) {
            const item = surveyTableId.rowItem(this, context, row)

            let box = null
            tryVerify(() => {
                box = findChild(item, "dataBox." + row + "." + cellRole)
                return box !== null
            }, 5000, "row " + row + " should have a box for cell " + cellRole)
            return box
        }

        function splayReading(context, row, cellRole) {
            return cellBox(context, row, cellRole).dataValue.reading.value
        }

        function splayChip(context, indexInChunk) {
            const cell = splaysCell(context, indexInChunk)

            let chip = null
            tryVerify(() => {
                chip = findChild(cell, "splayChip")
                return chip !== null
            }, 5000, "station " + indexInChunk + " should have a splay chip")
            return chip
        }

        function test_chipCountsTheStationsSplays() {
            const context = gotoSurveyTable()

            compare(findChild(splayChip(context, 0), "splayChipCount").text, "3")

            // A2 has no splays, so its cell never builds a chip at all
            compare(findChild(splaysCell(context, 1), "splayChip"), null)
        }

        // The chunk's trailing blank station stands for a station that hasn't
        // been surveyed yet, so it can hold no splays. Its Splays cell offers
        // the "+" to no one, while a real station with an empty cluster does.
        function test_theBlankStationOffersNoSplayHint() {
            const context = gotoSurveyTable()

            // Focusing a cell in the chunk is what brings the trailing blank
            // station and shot rows in
            context.editorModel.setFocusedCell(
                        context.editorModel.cellIndex(surveyTableId.stationRow(context, 0),
                                                      SurveyChunk.StationNameRole))
            tryCompare(context.editorModel, "focusedRow", surveyTableId.stationRow(context, 0))

            const realCell = splaysCell(context, 1)
            mouseMove(realCell, realCell.width * 0.5, realCell.height * 0.5)
            tryVerify(() => realCell.hovered, 5000, "A2's Splays cell should take the hover")
            tryVerify(() => findChild(realCell, "splayEntryButton") !== null, 5000,
                      "hovering A2's empty Splays cell should offer the +")

            const blankStation = context.chunk.stationCount
            const blankCell = splaysCell(context, blankStation)
            const rowsWhileClosed = context.view.count

            mouseMove(blankCell, blankCell.width * 0.5, blankCell.height * 0.5)
            tryVerify(() => blankCell.hovered, 5000,
                      "the blank station's Splays cell should take the hover")
            verify(findChild(blankCell, "splayEntryButton") === null,
                   "the blank station's Splays cell should offer nothing")

            mouseClick(blankCell)
            wait(settleMilliseconds)
            compare(context.view.count, rowsWhileClosed,
                    "clicking the blank station's Splays cell should open no cluster")
        }

        function test_clickingTheChipOpensTheSplayRows() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count

            mouseClick(splayChip(context, 0))

            tryCompare(context.view, "count", rowsWhileClosed + 3 + blankRow + virtualRows, 5000,
                       "the three splays should each become a row")

            const firstSplayRow = surveyTableId.stationRow(context, 0) + 1
            for (let i = 0; i < a4Splays.length; i++) {
                const item = surveyTableId.rowItem(this, context, firstSplayRow + i)
                compare(findChild(item, "splayRowLabel").text, "A1 · s" + (i + 1))
                compare(splayReading(context, firstSplayRow + i,
                                     SurveyEditorCellIndex.SplayDistanceCell),
                        a4Splays[i].distance)
                compare(splayReading(context, firstSplayRow + i,
                                     SurveyEditorCellIndex.SplayCompassCell),
                        a4Splays[i].compass)
                compare(splayReading(context, firstSplayRow + i,
                                     SurveyEditorCellIndex.SplayClinoCell),
                        a4Splays[i].clino)
            }
        }

        function test_clickingTheChipAgainClosesTheSplayRows() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count

            mouseClick(splayChip(context, 0))
            tryCompare(context.view, "count", rowsWhileClosed + 3 + blankRow + virtualRows)

            mouseClick(splayChip(context, 0))
            tryCompare(context.view, "count", rowsWhileClosed + virtualRows, 5000,
                       "closing the cluster should take the splay rows back out")
        }

        // The Splays cell is part of the table's tab chain: it comes after D on
        // the station row, and Enter on it opens the cluster the same way a
        // click does.
        function test_theSplaysCellTakesFocusAndOpensFromTheKeyboard() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count
            const stationRow = surveyTableId.stationRow(context, 0)

            context.editorModel.setFocusedCell(
                        context.editorModel.cellIndex(stationRow, SurveyChunk.StationDownRole))
            tryCompare(context.editorModel, "focusedRole", SurveyChunk.StationDownRole)

            const downBox = findChild(surveyTableId.rowItem(this, context, stationRow),
                                      "dataBox." + stationRow + "." + SurveyChunk.StationDownRole)
            verify(downBox !== null, "station 0 should have a D box")
            keyClick(Qt.Key_Tab)

            tryCompare(context.editorModel, "focusedRole", SurveyEditorCellIndex.StationSplaysCell, 5000,
                       "tabbing out of D should land on the Splays cell")
            tryCompare(context.editorModel, "focusedRow", stationRow)

            keyClick(Qt.Key_Return)
            tryCompare(context.view, "count", rowsWhileClosed + 3 + blankRow + virtualRows, 5000,
                       "Enter on the Splays cell should open the cluster")
        }

        // Each rail bridges into the row below so the cluster draws one
        // unbroken line, except the blank row at the bottom — below it is the
        // shot row's bare background, where the bridge would show as a loose
        // sliver.
        function test_theRailStopsAtTheBlankRow() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count

            mouseClick(splayChip(context, 0))
            tryCompare(context.view, "count", rowsWhileClosed + 3 + blankRow + virtualRows, 5000)

            const firstSplayRow = surveyTableId.stationRow(context, 0) + 1
            for (let i = 0; i < a4Splays.length; i++) {
                const item = surveyTableId.rowItem(this, context, firstSplayRow + i)
                const rail = findChild(item, "splayRail")
                verify(rail !== null, "splay row " + i + " should have a rail")
                verify(rail.height > item.height,
                       "splay row " + i + "'s rail should bridge into the row below")
            }

            const blankItem = surveyTableId.rowItem(this, context,
                                                    firstSplayRow + a4Splays.length)
            const blankRail = findChild(blankItem, "splayRail")
            verify(blankRail !== null, "the blank row should have a rail")
            compare(blankRail.height, blankItem.height,
                    "the blank row's rail should stop at its own row")
        }

        // The Splays cell offers the station's actions like the rest of the
        // row, rather than swallowing the click that asks for them.
        function test_rightClickingTheCellPopsTheStationMenu() {
            const context = gotoSurveyTable()
            const cell = splaysCell(context, 0)

            mouseClick(cell, cell.width / 2, cell.height / 2, Qt.RightButton)

            let menu = null
            tryVerify(() => {
                menu = findChild(cell, "stationMenuRoot")
                return menu !== null
            }, 5000, "right clicking the Splays cell should pop the station menu")
        }

        // Opens the cluster on the given station and answers the model row its
        // first splay landed on.
        function openCluster(context, indexInChunk) {
            const rowsWhileClosed = context.view.count

            mouseClick(splayChip(context, indexInChunk))
            tryCompare(context.view, "count", rowsWhileClosed + a4Splays.length + blankRow + virtualRows, 5000,
                       "the cluster should open")

            return surveyTableId.stationRow(context, indexInChunk) + 1
        }

        function splayCount(context, indexInChunk) {
            const row = surveyTableId.stationRow(context, indexInChunk)
            return context.editorModel.data(context.editorModel.index(row, 0),
                                            SurveyEditorModel.StationSplayCountRole)
        }

        // Triggering a menu item runs what a click on it runs — popup() under
        // offscreen rendering is what's unreliable, not the handler.
        function menuItem(parentItem, itemName) {
            let item = null
            tryVerify(() => {
                item = findChild(parentItem, itemName)
                return item !== null
            }, 5000, "the menu should offer " + itemName)
            return item
        }

        // The one bad shot out of a download goes at once, from the row's own
        // menu — there's nothing to confirm while the cluster it left is in view.
        function test_removingOneSplayTakesItsRow() {
            const context = gotoSurveyTable()
            const firstSplayRow = openCluster(context, 0)
            const openRows = context.view.count

            const row = surveyTableId.rowItem(this, context, firstSplayRow)
            verify(findChild(row, "splayRowMenuButton") !== null,
                   "a splay row should offer a ⋯ that opens its menu")

            const menu = findChild(row, "splayRowMenu")
            verify(menu !== null, "a splay row should carry a menu")
            menuItem(menu, "removeSplayMenuItem").triggered()

            tryCompare(context.view, "count", openRows - 1, 5000,
                       "the removed splay should take its row with it")
            compare(splayCount(context, 0), a4Splays.length - 1)

            // The rows that are left renumber, so the tags stay a count of the
            // cluster rather than of what it once held
            for (let i = 0; i < a4Splays.length - 1; i++) {
                const item = surveyTableId.rowItem(this, context, firstSplayRow + i)
                compare(findChild(item, "splayRowLabel").text, "A1 · s" + (i + 1))
                compare(splayReading(context, firstSplayRow + i,
                                     SurveyEditorCellIndex.SplayDistanceCell),
                        a4Splays[i + 1].distance)
            }
        }

        // Losing a whole cluster is the destructive one, so it asks first.
        function test_removingTheWholeClusterAsksFirst() {
            const context = gotoSurveyTable()
            openCluster(context, 0)
            const openRows = context.view.count

            const cell = splaysCell(context, 0)
            const clusterMenuButton = findChild(cell, "splayClusterMenuButton")
            verify(clusterMenuButton !== null,
                   "the Splays cell should offer a ⋯ beside its chip")

            // A real click, so the cell behind the ⋯ stays out of it — the
            // menu describes the cluster the user is looking at
            mouseClick(clusterMenuButton)
            compare(context.view.count, openRows,
                    "clicking the ⋯ should leave the cluster as it was")

            const removeItem = menuItem(cell, "stationMenuRemoveSplays")
            compare(removeItem.text, "Remove all 3 splays")
            removeItem.triggered()

            const challenge = findChild(surveyTableId.surveyEditor(), "removeSplaysChallenge")
            verify(challenge !== null, "the table should own a remove challenge")
            tryVerify(() => challenge.visible, 5000,
                      "removing a cluster should ask before it takes anything")
            compare(challenge.removeName, "all 3 splays from A1")

            mouseClick(findChild(challenge, "cancelButton"))
            tryVerify(() => !challenge.visible, 5000)
            compare(splayCount(context, 0), a4Splays.length,
                    "backing out should leave the cluster alone")
            compare(context.view.count, openRows)

            clusterMenuButton.tapped()
            menuItem(cell, "stationMenuRemoveSplays").triggered()
            tryVerify(() => challenge.visible, 5000)

            mouseClick(findChild(challenge, "removeButton"))

            tryCompare(context.view, "count", openRows - a4Splays.length - blankRow, 5000,
                       "an emptied cluster should collapse")
            compare(splayCount(context, 0), 0)
            tryVerify(() => findChild(splaysCell(context, 0), "splayChip") === null, 5000,
                      "the chip should go with the splays it counted")
        }

        function stationNameBox(context, indexInChunk) {
            const row = surveyTableId.stationRow(context, indexInChunk)
            const item = surveyTableId.rowItem(this, context, row)

            let box = null
            tryVerify(() => {
                box = findChild(item, "dataBox." + row + "." + SurveyChunk.StationNameRole)
                return box !== null
            }, 5000, "station " + indexInChunk + " should have a name box")
            return box
        }

        // The headline scenario: a cluster downloaded onto the wrong station
        // moves to its neighbor in three taps — ⋯, Move all to…, tap station.
        function test_movingAClusterToTheStationTheUserPicks() {
            const context = gotoSurveyTable()
            openCluster(context, 0)

            const cell = splaysCell(context, 0)
            mouseClick(findChild(cell, "splayClusterMenuButton"))

            const moveItem = menuItem(cell, "stationMenuMoveSplays")
            compare(moveItem.text, "Move all 3 splays to…")
            moveItem.triggered()

            tryVerify(() => context.editorModel.splayMoveActive, 5000,
                      "the move should wait for a station to land on")
            compare(context.editorModel.splayMoveCount, a4Splays.length)

            const banner = findChild(surveyTableId.surveyEditor(), "splayMoveBanner")
            verify(banner !== null, "the table should say what the move is waiting for")
            tryVerify(() => banner.visible, 5000)
            compare(findChild(banner, "splayMoveBannerLabel").text,
                    "Click a station to move 3 splays from A1 — Esc cancels")

            // A2 can take them, A1 can't take back what it already has
            // The outline is built only while a move can land on the station,
            // so its absence is what says the station is no target
            const targetBox = stationNameBox(context, 1)
            let outline = null
            tryVerify(() => {
                outline = findChild(targetBox, "splayMoveTargetOutline")
                return outline !== null
            }, 5000, "the station the splays can land on should show it")
            compare(findChild(stationNameBox(context, 0), "splayMoveTargetOutline"), null,
                    "the station the splays are leaving is no target")

            // Every stroke stays inside the station cell, so the cell next door
            // has nothing of ours to clip
            const outlineTopLeft = outline.mapToItem(targetBox, 0, 0)
            verify(outlineTopLeft.x >= targetBox.splayMoveOutlineInset
                   && outlineTopLeft.y >= targetBox.splayMoveOutlineInset,
                   "the outline should start inside the cell")
            verify(outlineTopLeft.x + outline.width <= targetBox.width - targetBox.splayMoveOutlineInset
                   && outlineTopLeft.y + outline.height <= targetBox.height - targetBox.splayMoveOutlineInset,
                   "the outline should end inside the cell")

            const rowsBeforeTheMove = context.view.count

            mouseClick(targetBox)

            tryVerify(() => !context.editorModel.splayMoveActive, 5000,
                      "landing the splays should end the move")
            compare(splayCount(context, 0), 0)
            compare(splayCount(context, 1), a4Splays.length)

            // A2 takes them without opening — the chip counting them is the
            // confirmation, and A1's emptied cluster is all the table loses
            tryCompare(context.view, "count",
                       rowsBeforeTheMove - a4Splays.length - blankRow, 5000,
                       "the target should take the splays without opening")
            tryVerify(() => findChild(splaysCell(context, 0), "splayChip") === null, 5000,
                      "the chip should go with the splays it counted")
            tryCompare(findChild(splayChip(context, 1), "splayChipCount"), "text",
                       String(a4Splays.length), 5000,
                       "the target's chip should count what it took")

            // And the splays are there for the asking
            const rowsWhileClosed = context.view.count
            mouseClick(splayChip(context, 1))
            tryCompare(context.view, "count",
                       rowsWhileClosed + a4Splays.length + blankRow, 5000,
                       "the target's chip should open on what it took")

            const firstSplayRow = surveyTableId.stationRow(context, 1) + 1
            for (let i = 0; i < a4Splays.length; i++) {
                const item = surveyTableId.rowItem(this, context, firstSplayRow + i)
                tryVerify(() => findChild(item, "splayRowLabel") !== null, 5000,
                          "the target should show splay row " + i)
                compare(findChild(item, "splayRowLabel").text, "A2 · s" + (i + 1))
                compare(splayReading(context, firstSplayRow + i,
                                     SurveyEditorCellIndex.SplayDistanceCell),
                        a4Splays[i].distance)
            }
        }

        // The chip is the handle for the cluster: the rest of the Splays cell
        // takes the focus like any other cell and leaves the rows alone.
        function test_clickingBesideTheChipLeavesTheClusterAlone() {
            const context = gotoSurveyTable()
            const cell = splaysCell(context, 0)
            const chip = splayChip(context, 0)
            const rowsWhileClosed = context.view.count

            // The near edge of the cell, clear of the chip and its ⋯
            const besideTheChip = chip.mapToItem(cell, 0, 0).x * 0.5
            verify(besideTheChip >= 1, "the chip should leave room to click beside it")

            mouseClick(cell, besideTheChip, cell.height * 0.5)

            // The focus is what brings the chunk's trailing virtual rows in, so
            // wait for it before the row count means anything
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.StationSplaysCell, 5000,
                       "the cell should still take the focus")
            wait(settleMilliseconds)

            compare(context.view.count, rowsWhileClosed + virtualRows,
                    "clicking beside the chip should open no cluster")
        }

        // A move armed by mistake costs one key to be rid of.
        function test_escCallsTheMoveOff() {
            const context = gotoSurveyTable()
            const firstSplayRow = openCluster(context, 0)

            const row = surveyTableId.rowItem(this, context, firstSplayRow)
            const menu = findChild(row, "splayRowMenu")
            verify(menu !== null, "a splay row should carry a menu")
            menuItem(menu, "moveSplayMenuItem").triggered()

            tryVerify(() => context.editorModel.splayMoveActive, 5000,
                      "one splay should be armed to move")
            compare(context.editorModel.splayMoveCount, 1)

            keyClick(Qt.Key_Escape)

            tryVerify(() => !context.editorModel.splayMoveActive, 5000,
                      "Esc should call the move off")
            compare(splayCount(context, 0), a4Splays.length)
        }

        // Every control in the cell answers to an armed move first, the ⋯
        // included: while a move waits for a station, clicking it calls the
        // move off rather than popping the station menu over it.
        function test_theClusterMenuButtonCallsAnArmedMoveOff() {
            const context = gotoSurveyTable()
            const firstSplayRow = openCluster(context, 0)

            const row = surveyTableId.rowItem(this, context, firstSplayRow)
            const rowMenu = findChild(row, "splayRowMenu")
            verify(rowMenu !== null, "a splay row should carry a menu")
            menuItem(rowMenu, "moveSplayMenuItem").triggered()

            tryVerify(() => context.editorModel.splayMoveActive, 5000,
                      "one splay should be armed to move")

            const cell = splaysCell(context, 0)
            mouseClick(findChild(cell, "splayClusterMenuButton"))

            tryVerify(() => !context.editorModel.splayMoveActive, 5000,
                      "clicking the ⋯ during a move should call the move off")
            wait(settleMilliseconds)
            compare(findChild(cell, "stationMenuRoot"), null,
                    "the station menu should stay shut while a move is armed")
            compare(splayCount(context, 0), a4Splays.length,
                    "a called-off move should leave the cluster alone")
        }

        // The splay row's own ⋯ answers to an armed move the same way the
        // cluster's does, rather than popping its menu over the move.
        function test_theRowMenuButtonCallsAnArmedMoveOff() {
            const context = gotoSurveyTable()
            const firstSplayRow = openCluster(context, 0)

            const row = surveyTableId.rowItem(this, context, firstSplayRow)
            const rowMenu = findChild(row, "splayRowMenu")
            verify(rowMenu !== null, "a splay row should carry a menu")
            menuItem(rowMenu, "moveSplayMenuItem").triggered()

            tryVerify(() => context.editorModel.splayMoveActive, 5000,
                      "one splay should be armed to move")

            mouseClick(findChild(row, "splayRowMenuButton"))

            tryVerify(() => !context.editorModel.splayMoveActive, 5000,
                      "clicking the row's ⋯ during a move should call the move off")
            compare(splayCount(context, 0), a4Splays.length,
                    "a called-off move should leave the cluster alone")
        }

        // Removing a station takes its splays with it, so the preview strikes
        // through the Splays cell and the cluster's rows, not just the readings.
        function test_removingTheStationPreviewsOverTheSplays() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count
            const cell = splaysCell(context, 0)

            mouseClick(splayChip(context, 0))
            tryCompare(context.view, "count", rowsWhileClosed + 3 + blankRow + virtualRows, 5000,
                       "the cluster should be open before the preview runs")

            verify(!cell.removePreviewActive,
                   "nothing is being removed yet")

            cell.removePreview.previewStation(context.chunk, 0, SurveyChunk.Below)

            tryVerify(() => cell.removePreviewActive, 5000,
                      "the Splays cell should show the station's remove preview")

            const firstSplayRow = surveyTableId.stationRow(context, 0) + 1
            for (let i = 0; i < a4Splays.length; i++) {
                const item = surveyTableId.rowItem(this, context, firstSplayRow + i)
                const line = findChild(item, "splayRemovePreviewLine")
                verify(line !== null, "splay row " + i + " should have a preview line")
                tryVerify(() => line.visible, 5000,
                          "splay row " + i + " should show the remove preview")
            }

            cell.removePreview.clear()
            tryVerify(() => !cell.removePreviewActive, 5000,
                      "clearing the preview should take the strike-through back off")
        }

        // A reading mistyped on the way out of a DistoX is corrected where it's
        // read: click it, type over it, Tab.
        function test_typingOverAReadingCommitsIt() {
            const context = gotoSurveyTable()
            const firstSplayRow = openCluster(context, 0)

            mouseClick(cellBox(context, firstSplayRow, SurveyEditorCellIndex.SplayCompassCell))
            tryCompare(context.editorModel, "focusedRow", firstSplayRow, 5000,
                       "clicking a splay reading should focus it")
            tryCompare(context.editorModel, "focusedRole", SurveyEditorCellIndex.SplayCompassCell)

            // The first key opens the editor over the cell, the rest go into it
            keyClick(Qt.Key_1)
            keyClick(Qt.Key_1)
            keyClick(Qt.Key_9)
            keyClick(Qt.Key_Period)
            keyClick(Qt.Key_4)
            keyClick(Qt.Key_Tab)

            tryVerify(() => splayReading(context, firstSplayRow,
                                         SurveyEditorCellIndex.SplayCompassCell) === "119.4",
                      5000, "tabbing out of the cell should commit what was typed")
            tryCompare(context.editorModel, "focusedRole", SurveyEditorCellIndex.SplayClinoCell, 5000,
                       "tab should carry on to the next reading of the same splay")

            // The splays around it keep the readings they were downloaded with
            compare(splayReading(context, firstSplayRow, SurveyEditorCellIndex.SplayDistanceCell),
                    a4Splays[0].distance)
            compare(splayReading(context, firstSplayRow + 1, SurveyEditorCellIndex.SplayCompassCell),
                    a4Splays[1].compass)
        }

        // A key the reading can't hold never opens the editor, so the splay
        // keeps what it had.
        function test_aRejectedKeyLeavesTheReadingAlone() {
            const context = gotoSurveyTable()
            const firstSplayRow = openCluster(context, 0)

            mouseClick(cellBox(context, firstSplayRow, SurveyEditorCellIndex.SplayDistanceCell))
            tryCompare(context.editorModel, "focusedRole", SurveyEditorCellIndex.SplayDistanceCell, 5000)

            keyClick(Qt.Key_X)
            keyClick(Qt.Key_Tab)

            compare(splayReading(context, firstSplayRow, SurveyEditorCellIndex.SplayDistanceCell),
                    a4Splays[0].distance)
        }

        // The chunk's first shot row, for the space cases below.
        function shotRow(context, indexInChunk) {
            const row = context.editorModel.toModelRow(
                          context.editorModel.rowIndex(context.chunk, indexInChunk,
                                                       SurveyEditorRowIndex.ShotRow))
            verify(row >= 0, "shot " + indexInChunk + " should have a model row")
            return row
        }

        // The half of the guard that has to keep working: space on a shot cell
        // still starts the trip's next chunk.
        function test_spaceOnAShotReadingStartsTheNextChunk() {
            const context = gotoSurveyTable()

            mouseClick(cellBox(context, shotRow(context, 0),
                               SurveyEditorCellIndex.ShotDistanceCell))
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.ShotDistanceCell, 5000,
                       "clicking a shot reading should focus it")

            keyClick(Qt.Key_Space)
            tryCompare(context.trip, "chunkCount", 2, 5000,
                       "space on a shot cell should start the trip's next chunk")
        }

        // The same through a shot cell's open editor: space commits what was
        // typed and carries on into the next chunk.
        function test_spaceOutOfAShotEditorStartsTheNextChunk() {
            const context = gotoSurveyTable()
            const row = shotRow(context, 0)

            mouseClick(cellBox(context, row, SurveyEditorCellIndex.ShotDistanceCell))
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.ShotDistanceCell, 5000,
                       "clicking a shot reading should focus it")

            keyClick(Qt.Key_7)
            keyClick(Qt.Key_Space)

            tryVerify(() => cellBox(context, row, SurveyEditorCellIndex.ShotDistanceCell)
                              .dataValue.reading.value === "7",
                      5000, "space should commit the reading the editor holds")
            tryCompare(context.trip, "chunkCount", 2, 5000,
                       "space out of a shot's editor should start the trip's next chunk")
        }

        // Space on a shot cell starts the trip's next chunk. A splay's readings
        // stand outside that flow, so space there leaves the trip alone —
        // whether the cell is sitting there or has its editor open.
        function test_spaceOnASplayReadingStartsNoChunk() {
            const context = gotoSurveyTable()
            const firstSplayRow = openCluster(context, 0)

            mouseClick(cellBox(context, firstSplayRow, SurveyEditorCellIndex.SplayDistanceCell))
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.SplayDistanceCell, 5000,
                       "clicking a splay reading should focus it")

            keyClick(Qt.Key_Space)
            wait(settleMilliseconds)
            compare(context.trip.chunkCount, 1,
                    "space on a splay cell should start no chunk")

            // The same key through the open editor: it commits what was typed
            // and stops there
            keyClick(Qt.Key_7)
            keyClick(Qt.Key_Space)
            tryVerify(() => splayReading(context, firstSplayRow,
                                         SurveyEditorCellIndex.SplayDistanceCell) === "7",
                      5000, "space should commit the reading the editor holds")
            wait(settleMilliseconds)
            compare(context.trip.chunkCount, 1,
                    "space out of a splay's editor should start no chunk")
        }

        // The manual-entry scenario: the Bluetooth link is dead, so a station
        // with no splays takes them from the keyboard — Enter on its Splays
        // cell, then the three readings with Tab between them.
        function test_typingASplayIntoTheBlankRow() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count
            const stationRow = surveyTableId.stationRow(context, 1)

            // A2 carries no splays, so its cell opens on the blank row alone
            mouseClick(splaysCell(context, 1))
            tryCompare(context.view, "count", rowsWhileClosed + blankRow + virtualRows, 5000,
                       "a station with no splays should open on its blank row")

            const entryRow = stationRow + 1
            tryCompare(context.editorModel, "focusedRow", entryRow, 5000,
                       "opening an empty cluster should put the caret in the blank row")
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.SplayDistanceCell)
            compare(findChild(surveyTableId.rowItem(this, context, entryRow),
                              "splayRowLabel").text, "A2 · +")

            const typeReading = function(keys) {
                for (let i = 0; i < keys.length; i++) {
                    keyClick(keys[i])
                }
                keyClick(Qt.Key_Tab)
            }

            typeReading([Qt.Key_4, Qt.Key_Period, Qt.Key_2])
            typeReading([Qt.Key_1, Qt.Key_2, Qt.Key_0])
            typeReading([Qt.Key_5])

            tryVerify(() => splayCount(context, 1) === 1, 5000,
                      "the readings typed into the blank row should make a splay")
            tryCompare(context.view, "count",
                       rowsWhileClosed + 1 + blankRow + virtualRows, 5000,
                       "a fresh blank row should wait under the splay that was typed")

            compare(splayReading(context, entryRow, SurveyEditorCellIndex.SplayDistanceCell), "4.2")
            compare(splayReading(context, entryRow, SurveyEditorCellIndex.SplayCompassCell), "120")
            compare(splayReading(context, entryRow, SurveyEditorCellIndex.SplayClinoCell), "5")

            // The row that was blank holds a splay now, and the one below it is
            // where the next one goes
            compare(findChild(surveyTableId.rowItem(this, context, entryRow),
                              "splayRowLabel").text, "A2 · s1")
            compare(findChild(surveyTableId.rowItem(this, context, entryRow + 1),
                              "splayRowLabel").text, "A2 · +")
        }

        // The "+" is a button, not a hint: it's round, it's a target rather
        // than a character, and clicking it opens the row a splay is typed
        // into. Same manual-entry path as the keyboard's, reached by mouse.
        function test_clickingTheEntryButtonTakesASplay() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count
            const cell = splaysCell(context, 1)

            mouseMove(cell, cell.width * 0.5, cell.height * 0.5)
            tryVerify(() => cell.hovered, 5000, "A2's Splays cell should take the hover")

            let button = null
            tryVerify(() => {
                button = findChild(cell, "splayEntryButton")
                return button !== null
            }, 5000, "hovering an empty Splays cell should offer the + button")

            compare(button.width, Theme.splayEntryButtonSize)
            compare(button.height, Theme.splayEntryButtonSize)
            compare(button.radius, Theme.splayEntryButtonSize * 0.5,
                    "the entry button should be round")

            mouseClick(button)

            const entryRow = surveyTableId.stationRow(context, 1) + 1
            tryCompare(context.view, "count", rowsWhileClosed + blankRow + virtualRows, 5000,
                       "clicking the + should open the row a splay is typed into")
            tryCompare(context.editorModel, "focusedRow", entryRow, 5000,
                       "clicking the + should put the caret in the blank row")
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.SplayDistanceCell)

            // The model's focus lands a frame before the cell holding it takes
            // the keyboard, and the first reading's first key is what would go
            // missing in between
            const distanceBox = cellBox(context, entryRow,
                                         SurveyEditorCellIndex.SplayDistanceCell)
            tryVerify(() => distanceBox.activeFocus, 5000,
                      "the blank row's distance cell should have the keyboard")

            const typeReading = function(keys) {
                for (let i = 0; i < keys.length; i++) {
                    keyClick(keys[i])
                }
                keyClick(Qt.Key_Tab)
            }

            typeReading([Qt.Key_3, Qt.Key_Period, Qt.Key_1])
            typeReading([Qt.Key_9, Qt.Key_0])
            typeReading([Qt.Key_Minus, Qt.Key_7])

            tryVerify(() => splayCount(context, 1) === 1, 5000,
                      "the readings typed after the + should make a splay")
            compare(splayReading(context, entryRow, SurveyEditorCellIndex.SplayDistanceCell), "3.1")
            compare(splayReading(context, entryRow, SurveyEditorCellIndex.SplayCompassCell), "90")
            compare(splayReading(context, entryRow, SurveyEditorCellIndex.SplayClinoCell), "-7")

            // The station carries splays now, so the cell trades the button for
            // the chip that counts them
            tryVerify(() => findChild(splaysCell(context, 1), "splayChip") !== null, 5000,
                      "the chip should take the button's place")
            compare(findChild(splaysCell(context, 1), "splayEntryButton"), null,
                    "a station with splays offers no + button")
        }

        // Tabbing across a blank row that was never typed into leaves it blank
        // and carries on to the next station, the way the table tabs today.
        function test_tabbingThroughTheBlankRowReachesTheNextStation() {
            const context = gotoSurveyTable()
            const firstSplayRow = openCluster(context, 0)
            const blankRowIndex = firstSplayRow + a4Splays.length

            mouseClick(cellBox(context, blankRowIndex, SurveyEditorCellIndex.SplayDistanceCell))
            tryCompare(context.editorModel, "focusedRow", blankRowIndex, 5000,
                       "clicking the blank row should focus it")

            keyClick(Qt.Key_Tab)
            keyClick(Qt.Key_Tab)
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.SplayClinoCell, 5000)

            keyClick(Qt.Key_Tab)

            // The cluster is a pass-through: tab leaves it the way the Splays
            // cell does, and the first station's row runs into the second
            // station's LRUD rather than its name
            tryCompare(context.editorModel, "focusedRow",
                       surveyTableId.stationRow(context, 1), 5000,
                       "tab out of an untouched blank row should reach the next station")
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.StationLeftCell)
            compare(splayCount(context, 0), a4Splays.length,
                    "tabbing through the blank row should make no splay")
        }

        // The arrows run the cluster the way they run the rest of the grid:
        // across the three readings, down the rows, and out the top onto the
        // cell the cluster hangs from.
        function test_arrowsWalkTheClusterReadings() {
            const context = gotoSurveyTable()
            const firstSplayRow = openCluster(context, 0)

            mouseClick(cellBox(context, firstSplayRow, SurveyEditorCellIndex.SplayDistanceCell))
            tryCompare(context.editorModel, "focusedRole", SurveyEditorCellIndex.SplayDistanceCell, 5000)

            keyClick(Qt.Key_Right)
            tryCompare(context.editorModel, "focusedRole", SurveyEditorCellIndex.SplayCompassCell, 5000)
            tryCompare(context.editorModel, "focusedRow", firstSplayRow)

            keyClick(Qt.Key_Down)
            tryCompare(context.editorModel, "focusedRow", firstSplayRow + 1, 5000,
                       "down should stay in the column and move a splay along")
            tryCompare(context.editorModel, "focusedRole", SurveyEditorCellIndex.SplayCompassCell)

            keyClick(Qt.Key_Up)
            keyClick(Qt.Key_Up)
            tryCompare(context.editorModel, "focusedRow",
                       surveyTableId.stationRow(context, 0), 5000,
                       "up out of the cluster should land on the cell it hangs from")
            tryCompare(context.editorModel, "focusedRole", SurveyEditorCellIndex.StationSplaysCell)
        }

        // The cluster is part of the tab chain: tab off the Splays cell walks
        // into it, and shift-tab walks the same cells back the other way.
        function test_tabWalksIntoTheClusterAndBackOut() {
            const context = gotoSurveyTable()
            const firstSplayRow = openCluster(context, 0)
            const blankRowIndex = firstSplayRow + a4Splays.length
            const stationRow = surveyTableId.stationRow(context, 0)

            context.editorModel.setFocusedCell(
                        context.editorModel.cellIndex(stationRow,
                                                      SurveyEditorCellIndex.StationSplaysCell))
            tryCompare(context.editorModel, "focusedRow", stationRow, 5000)
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.StationSplaysCell)

            keyClick(Qt.Key_Tab)
            tryCompare(context.editorModel, "focusedRow", firstSplayRow, 5000,
                       "tab off the Splays cell should enter the cluster it has open")
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.SplayDistanceCell)

            keyClick(Qt.Key_Backtab, Qt.ShiftModifier)
            tryCompare(context.editorModel, "focusedRow", stationRow, 5000,
                       "shift-tab should hand the caret back to the cell it came from")
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.StationSplaysCell)

            // The next station's LRUD comes after the cluster in the chain, so
            // shift-tab off it reaches the blank row at the cluster's bottom
            context.editorModel.setFocusedCell(
                        context.editorModel.cellIndex(surveyTableId.stationRow(context, 1),
                                                      SurveyChunk.StationLeftRole))
            tryCompare(context.editorModel, "focusedRole", SurveyChunk.StationLeftRole, 5000)

            keyClick(Qt.Key_Backtab, Qt.ShiftModifier)
            tryCompare(context.editorModel, "focusedRow", blankRowIndex, 5000,
                       "shift-tab into a station row should land in the cluster above it")
            tryCompare(context.editorModel, "focusedRole",
                       SurveyEditorCellIndex.SplayClinoCell)
        }
    }
}
