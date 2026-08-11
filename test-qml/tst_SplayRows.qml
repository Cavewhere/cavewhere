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

        function test_clickingTheCellOpensTheSplayRows() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count

            mouseClick(splaysCell(context, 0))

            tryCompare(context.view, "count", rowsWhileClosed + 3 + virtualRows, 5000,
                       "the three splays should each become a row")

            const firstSplayRow = surveyTableId.stationRow(context, 0) + 1
            for (let i = 0; i < a4Splays.length; i++) {
                const item = surveyTableId.rowItem(this, context, firstSplayRow + i)
                compare(findChild(item, "splayRowLabel").text, "A1 · s" + (i + 1))
                compare(findChild(item, "splayDistanceLabel").text, a4Splays[i].distance)
                compare(findChild(item, "splayCompassLabel").text, a4Splays[i].compass)
                compare(findChild(item, "splayClinoLabel").text, a4Splays[i].clino)
            }
        }

        function test_clickingTheCellAgainClosesTheSplayRows() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count

            mouseClick(splaysCell(context, 0))
            tryCompare(context.view, "count", rowsWhileClosed + 3 + virtualRows)

            mouseClick(splaysCell(context, 0))
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
            tryCompare(context.view, "count", rowsWhileClosed + 3 + virtualRows, 5000,
                       "Enter on the Splays cell should open the cluster")
        }

        // Each rail bridges into the row below so the cluster draws one
        // unbroken line, except the last — below it is the shot row's bare
        // background, where the bridge would show as a loose sliver.
        function test_theRailStopsAtTheLastSplay() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count

            mouseClick(splaysCell(context, 0))
            tryCompare(context.view, "count", rowsWhileClosed + 3 + virtualRows, 5000)

            const firstSplayRow = surveyTableId.stationRow(context, 0) + 1
            for (let i = 0; i < a4Splays.length; i++) {
                const item = surveyTableId.rowItem(this, context, firstSplayRow + i)
                const rail = findChild(item, "splayRail")
                verify(rail !== null, "splay row " + i + " should have a rail")

                const lastSplay = i + 1 === a4Splays.length
                if(lastSplay) {
                    compare(rail.height, item.height,
                            "the last splay's rail should stop at its own row")
                } else {
                    verify(rail.height > item.height,
                           "splay row " + i + "'s rail should bridge into the row below")
                }
            }
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

            mouseClick(splaysCell(context, indexInChunk))
            tryCompare(context.view, "count", rowsWhileClosed + a4Splays.length + virtualRows, 5000,
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
                compare(findChild(item, "splayDistanceLabel").text, a4Splays[i + 1].distance)
            }
        }

        // Losing a whole cluster is the destructive one, so it asks first.
        function test_deletingTheWholeClusterAsksFirst() {
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

            const deleteItem = menuItem(cell, "stationMenuDeleteSplays")
            compare(deleteItem.text, "Delete all 3 splays")
            deleteItem.triggered()

            const challenge = findChild(surveyTableId.surveyEditor(), "removeSplaysChallenge")
            verify(challenge !== null, "the table should own a remove challenge")
            tryVerify(() => challenge.visible, 5000,
                      "deleting a cluster should ask before it takes anything")
            compare(challenge.removeName, "all 3 splays from A1")

            mouseClick(findChild(challenge, "cancelButton"))
            tryVerify(() => !challenge.visible, 5000)
            compare(splayCount(context, 0), a4Splays.length,
                    "backing out should leave the cluster alone")
            compare(context.view.count, openRows)

            clusterMenuButton.tapped(Qt.point(0, 0))
            menuItem(cell, "stationMenuDeleteSplays").triggered()
            tryVerify(() => challenge.visible, 5000)

            mouseClick(findChild(challenge, "removeButton"))

            tryCompare(context.view, "count", openRows - a4Splays.length, 5000,
                       "an emptied cluster should collapse")
            compare(splayCount(context, 0), 0)
            tryVerify(() => findChild(splaysCell(context, 0), "splayChip") === null, 5000,
                      "the chip should go with the splays it counted")
        }

        // Removing a station takes its splays with it, so the preview strikes
        // through the Splays cell and the cluster's rows, not just the readings.
        function test_removingTheStationPreviewsOverTheSplays() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count
            const cell = splaysCell(context, 0)

            mouseClick(cell)
            tryCompare(context.view, "count", rowsWhileClosed + 3 + virtualRows, 5000,
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
    }
}
