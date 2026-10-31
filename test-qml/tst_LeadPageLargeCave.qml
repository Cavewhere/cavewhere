import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// A cave with many leads opens its Leads page with the table building a
// screenful of rows, not one row per lead. The view must be the first Leads
// page this file opens: the page item is reused afterward, so only a cold open
// shows the first fill. The check runs in the same turn the page is built,
// since rows built past the screen are released at the next layout and would
// no longer show.
MainWindowTest {
    id: rootId

    // Waits for the leads to load without building the Leads page
    LeadModel {
        id: caveLeadsId
        regionModel: RootData.regionTreeModel
    }

    TestCase {
        name: "LeadPageLargeCave"
        when: windowShown

        readonly property int noteCount: 10
        readonly property int leadsPerNote: 100
        readonly property int leadCount: noteCount * leadsPerNote

        // A screenful of rows with the view's cache buffer, well under the
        // 1k leads the cave holds
        readonly property int maxBuiltRows: 200

        function builtRowCount(view) {
            let rows = 0
            for (const child of view.contentItem.children) {
                if (child.leadDescription !== undefined) {
                    rows++
                }
            }
            return rows
        }

        function test_coldOpenBuildsVisibleRowsOnly() {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = "LargeLeadCave"
            cave.addTrip()
            const trip = cave.trip(0)
            trip.name = "LargeLeadTrip"

            for (let n = 0; n < noteCount; n++) {
                const note = TestHelper.addNoteWithScrap(trip, "note" + n)
                verify(note !== null, "note " + n + " should be added with a scrap")
                verify(TestHelper.addScrapStation(note, 0, "a" + n, Qt.point(0.1, 0.1)))
                verify(TestHelper.addScrapStation(note, 0, "b" + n, Qt.point(0.9, 0.9)))
                for (let i = 0; i < leadsPerNote; i++) {
                    const along = (i + 1) / (leadsPerNote + 1)
                    verify(TestHelper.addScrapLead(note, 0, Qt.point(along, along), Qt.size(1, 1),
                                                   "lead " + n + "-" + i))
                }
            }

            caveLeadsId.cave = cave
            tryCompare(caveLeadsId, "count", leadCount, 10000, "the cave should list every lead")

            RootData.pageSelectionModel.currentPageAddress =
                    "Source/Data/Node=" + cave.name + "/Leads"
            const view = ObjectFinder.findObjectByChain(rootId.mainWindow,
                                                        "rootId->leadPage->leadTableView")
            verify(view !== null, "the page should build in the turn it is opened")

            compare(view.count, leadCount, "the table should hold a row per lead")
            verify(builtRowCount(view) <= maxBuiltRows,
                   "built " + builtRowCount(view) + " of " + view.count + " rows")
            tryVerify(() => builtRowCount(view) > 0, 5000, "the view should fill its first screen")
        }
    }
}
