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

            tryCompare(context.editorModel, "focusedRole", SurveyChunk.StationSplaysRole, 5000,
                       "tabbing out of D should land on the Splays cell")
            tryCompare(context.editorModel, "focusedRow", stationRow)

            keyClick(Qt.Key_Return)
            tryCompare(context.view, "count", rowsWhileClosed + 3 + virtualRows, 5000,
                       "Enter on the Splays cell should open the cluster")
        }
    }
}
