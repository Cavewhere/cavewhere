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

        function splayChip(context, indexInChunk) {
            const item = surveyTableId.rowItem(
                           this, context, surveyTableId.stationRow(context, indexInChunk))

            let chip = null
            tryVerify(() => {
                chip = findChild(item, "splayChip")
                return chip !== null
            }, 5000, "station " + indexInChunk + " should have a splay chip")
            return chip
        }

        function test_chipCountsTheStationsSplays() {
            const context = gotoSurveyTable()

            compare(findChild(splayChip(context, 0), "splayChipCount").text, "3")

            // A2 has no splays, so it never builds a chip at all
            const a2Row = surveyTableId.rowItem(
                            this, context, surveyTableId.stationRow(context, 1))
            compare(findChild(a2Row, "splayChip"), null)
        }

        function test_clickingTheChipOpensTheSplayRows() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count

            mouseClick(splayChip(context, 0))

            tryCompare(context.view, "count", rowsWhileClosed + 3, 5000,
                       "the three splays should each become a row")

            const firstSplayRow = surveyTableId.stationRow(context, 0) + 1
            for (let i = 0; i < a4Splays.length; i++) {
                const item = surveyTableId.rowItem(this, context, firstSplayRow + i)
                compare(findChild(item, "splayRowLabel").text, "splay")
                compare(findChild(item, "splayDistanceLabel").text, a4Splays[i].distance)
                compare(findChild(item, "splayCompassLabel").text, a4Splays[i].compass)
                compare(findChild(item, "splayClinoLabel").text, a4Splays[i].clino)
            }
        }

        function test_clickingTheChipAgainClosesTheSplayRows() {
            const context = gotoSurveyTable()
            const rowsWhileClosed = context.view.count

            mouseClick(splayChip(context, 0))
            tryCompare(context.view, "count", rowsWhileClosed + 3)

            mouseClick(splayChip(context, 0))
            tryCompare(context.view, "count", rowsWhileClosed, 5000,
                       "closing the chip should take the splay rows back out")
        }
    }
}
