import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// A large trip opens with the survey table building a screenful of rows, not
// every row of the trip. The view must be the first trip page this file opens:
// the page item is reused afterward, so only a cold open shows the first fill.
// The check runs in the same turn the page is built, since rows built past the
// screen are released at the next layout and would no longer show.
MainWindowTest {
    id: rootId

    TestCase {
        name: "SurveyEditorLargeTrip"
        when: windowShown

        readonly property int blockCount: 50
        readonly property int shotsPerBlock: 20

        // A screenful of rows with the view's cache buffer, well under the
        // ~2k rows the trip holds
        readonly property int maxBuiltRows: 200

        // Generated survey: blocks of shots chained A1, A2, ..., each block
        // tied into an earlier one
        function largeSurvex() {
            let lines = ["*begin alpha", "*units tape feet", ""]
            let n = 1
            for (let b = 0; b < blockCount; b++) {
                lines.push("*begin", "*data normal from to compass tape clino", "")
                const start = n
                for (let i = 0; i < shotsPerBlock; i++) {
                    const from = (i === 0 && b > 0) ? "A" + (start - 3) : "A" + n
                    lines.push(from + " A" + (n + 1) + " " + ((n * 37) % 360) + " "
                               + (5 + n % 20) + " " + ((n % 30) - 15))
                    n++
                }
                lines.push("", "*end", "")
            }
            lines.push("*end")
            return lines.join("\n") + "\n"
        }

        function builtRowCount(view) {
            let rows = 0
            for (const child of view.contentItem.children) {
                if (child.rowType !== undefined) {
                    rows++
                }
            }
            return rows
        }

        function test_coldOpenBuildsVisibleRowsOnly() {
            const dir = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            const path = dir + "/large.svx"
            verify(TestHelper.writeTextFile(path, largeSurvex()))

            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = "LargeCave"
            cave.addTrip()
            const trip = cave.trip(0)
            trip.name = "LargeTrip"
            RootData.surveyImportManager.importSurvexToTrip(TestHelper.toLocalUrl(path), trip)
            tryVerify(() => trip.chunkCount === blockCount, 30000, "the survey should import")

            RootData.pageSelectionModel.currentPageAddress =
                    "Source/Data/Node=" + cave.name + "/Trip=" + trip.name
            const view = ObjectFinder.findObjectByChain(rootId.mainWindow,
                                                        "rootId->tripPage->surveyEditor->view")
            verify(view !== null && view.model !== null && view.model.trip === trip,
                   "the page should build in the turn it is opened")

            verify(view.count >= blockCount * (2 * shotsPerBlock + 1),
                   "the trip should hold a row per station and shot")
            verify(builtRowCount(view) <= maxBuiltRows,
                   "built " + builtRowCount(view) + " of " + view.count + " rows")
            tryVerify(() => builtRowCount(view) > 0, 5000, "the view should fill its first screen")
        }
    }
}
