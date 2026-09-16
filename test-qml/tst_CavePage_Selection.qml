import QtQuick
import QtQml.Models
import QtTest
import cavewherelib
import cw.TestLib

// The cave page's tree picks rows the way every TableView does — a click, a
// ctrl-click, a shift-click — so what is tested here is what CaveWhere adds on
// top: which trips a verb on a row acts on, and that the set follows the rows
// as trips come and go.
MainWindowTest {
    id: rootId

    TestCase {
        name: "CavePageSelection"
        when: windowShown

        property var cave: null

        function initTestCase() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.region.addCave()
            cave = RootData.region.cave(0)
            cave.name = "TestCave"
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=TestCave"
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "cavePage")
        }

        function init() {
            cave.clearTrips()
            tree().selectionModel.clearSelection()
        }

        function cavePage() {
            let p = RootData.pageView.currentPageItem
            verify(p !== null, "cave page must exist")
            return p
        }

        // The tree is rooted at the cave, so every row of it is one of the
        // cave's trips.
        function tree() {
            let treeView = findChild(cavePage(), "tripTree")
            verify(treeView !== null, "the cave page must show the survey tree")
            return treeView
        }

        function setupCaveWithTrips() {
            for (let i = 0; i < 5; ++i) {
                cave.addTrip()
                cave.trip(i).name = "Trip-" + i
            }
            tryCompare(tree(), "rowCount", 5, 5000, "every trip takes a row")
            return cave
        }

        function selectRows(rows) {
            const treeView = tree()
            treeView.selectionModel.clearSelection()
            for (let i = 0; i < rows.length; ++i) {
                treeView.selectionModel.select(treeView.indexAtRow(rows[i]),
                                               ItemSelectionModel.Select
                                               | ItemSelectionModel.Rows)
            }
        }

        function selectedTripNames() {
            return tree().selectedTrips()
                .map(trip => String(trip.name))
                .sort()
        }

        // ── A row is picked whole, and each trip is counted once ─────────────

        // A row is selected one cell at a time, so the same trip arrives once
        // per column; the tree hands each of them back once.
        function test_selectingRowsNamesEachTripOnce() {
            setupCaveWithTrips()

            selectRows([1, 3])
            compare(selectedTripNames(), ["Trip-1", "Trip-3"])
        }

        // ── The scope of a row's verb ───────────────────────────────────────

        function test_aRowInsideTheSelectionCoversTheSelection() {
            setupCaveWithTrips()
            const treeView = tree()

            selectRows([1, 3, 4])

            const trips = treeView.tripsFor(treeView.objectAtRow(3))
            compare(trips.length, 3, "row 3 is in a multi-selection")
            verify(trips.indexOf(cave.trip(1)) >= 0)
            verify(trips.indexOf(cave.trip(3)) >= 0)
            verify(trips.indexOf(cave.trip(4)) >= 0)
        }

        function test_aRowOutsideTheSelectionCoversItself() {
            setupCaveWithTrips()
            const treeView = tree()

            selectRows([2])

            const trips = treeView.tripsFor(treeView.objectAtRow(0))
            compare(trips.length, 1, "a row nothing else is selected with stands alone")
            compare(trips[0], cave.trip(0))
        }

        // A single selected row is no multi-selection, even when it is the row
        // being asked about.
        function test_aSingletonSelectionStaysSingleRow() {
            setupCaveWithTrips()
            const treeView = tree()

            selectRows([2])

            const trips = treeView.tripsFor(treeView.objectAtRow(2))
            compare(trips.length, 1)
            compare(trips[0], cave.trip(2))
        }

        // ── The selection follows the rows ──────────────────────────────────

        function test_clearingEmptiesTheSelection() {
            setupCaveWithTrips()

            selectRows([1, 3])
            compare(selectedTripNames().length, 2)

            tree().selectionModel.clearSelection()
            compare(selectedTripNames(), [], "clear empties the set")
        }

        function test_removedTripsDropOutOfTheSelection() {
            setupCaveWithTrips()

            selectRows([1, 4])
            compare(selectedTripNames(), ["Trip-1", "Trip-4"])

            cave.removeTrip(4)
            tryCompare(tree(), "rowCount", 4, 5000)
            compare(selectedTripNames(), ["Trip-1"],
                    "a trip that no longer has a row leaves the selection")
        }

        // Removing an earlier row shifts the rows under it; the selection is
        // about trips, so the same trips stay in it.
        function test_selectionKeepsItsTripsWhenAnEarlierRowGoes() {
            setupCaveWithTrips()

            selectRows([1, 4])
            compare(selectedTripNames(), ["Trip-1", "Trip-4"])

            cave.removeTrip(0)
            tryCompare(tree(), "rowCount", 4, 5000)
            compare(selectedTripNames(), ["Trip-1", "Trip-4"],
                    "the rows moved up, the trips did not change")
        }

        function test_addingATripLeavesTheSelectionAlone() {
            setupCaveWithTrips()

            selectRows([1, 3])
            const before = selectedTripNames()

            cave.addTrip()
            tryCompare(tree(), "rowCount", 6, 5000)

            compare(selectedTripNames(), before,
                    "a new row must not change which trips are selected")
        }
    }
}
