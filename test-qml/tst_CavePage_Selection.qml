import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

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
            cavePage().selection.clear()
        }

        function setupCaveWithTrips() {
            cave.addTrip(); cave.trip(0).name = "Trip-0"
            cave.addTrip(); cave.trip(1).name = "Trip-1"
            cave.addTrip(); cave.trip(2).name = "Trip-2"
            cave.addTrip(); cave.trip(3).name = "Trip-3"
            cave.addTrip(); cave.trip(4).name = "Trip-4"
            return cave
        }

        function cavePage() {
            let p = RootData.pageView.currentPageItem
            verify(p !== null, "cave page must exist")
            return p
        }

        function selectedRows() {
            return cavePage().selection.selectedIndexes
                .map(i => i.row)
                .sort((a, b) => a - b)
        }

        function click(row, modifiers) {
            cavePage().applySelectionClick(row, modifiers)
        }

        // ── Plain click replaces selection ───────────────────────────────────

        function test_plainClickReplacesSelection() {
            setupCaveWithTrips()

            click(1, Qt.NoModifier)
            compare(selectedRows(), [1])

            click(3, Qt.NoModifier)
            compare(selectedRows(), [3], "plain click on a new row replaces")
        }

        // ── Ctrl/Cmd click toggles ───────────────────────────────────────────

        function test_ctrlClickToggles() {
            setupCaveWithTrips()

            click(1, Qt.NoModifier)
            click(3, Qt.ControlModifier)
            compare(selectedRows(), [1, 3], "ctrl-click adds")

            click(1, Qt.ControlModifier)
            compare(selectedRows(), [3], "ctrl-click on a selected row removes it")

            // Cmd on macOS shows up as MetaModifier in Qt — same semantics.
            click(0, Qt.MetaModifier)
            compare(selectedRows(), [0, 3])
        }

        // ── Shift click extends from anchor ──────────────────────────────────

        function test_shiftClickExtendsRange() {
            setupCaveWithTrips()

            click(1, Qt.NoModifier)
            click(3, Qt.ShiftModifier)
            compare(selectedRows(), [1, 2, 3], "shift extends forward")

            // Anchor stays at row 1; shifting backward to 0 yields [0, 1].
            click(0, Qt.ShiftModifier)
            compare(selectedRows(), [0, 1], "shift extends backward from anchor")
        }

        // ── Shift without anchor behaves like a plain click ──────────────────

        function test_shiftClickWithoutAnchorActsAsPlain() {
            setupCaveWithTrips()

            // Fresh selection, no current index.
            cavePage().selection.clear()

            click(2, Qt.ShiftModifier)
            compare(selectedRows(), [2], "shift without an anchor selects the single row")
        }

        // ── Clear empties the set ────────────────────────────────────────────

        function test_clearEmptiesSelection() {
            setupCaveWithTrips()

            click(1, Qt.NoModifier)
            click(3, Qt.ControlModifier)
            compare(selectedRows().length, 2)

            cavePage().selection.clear()
            compare(selectedRows(), [], "clear empties the set")

            // After a clear, plain click starts a fresh anchor.
            click(4, Qt.NoModifier)
            click(2, Qt.ShiftModifier)
            compare(selectedRows(), [2, 3, 4])
        }

        // ── Selection follows row removal from the END ───────────────────────

        function test_selectionDropsRowsRemovedFromEnd() {
            setupCaveWithTrips()

            click(1, Qt.NoModifier)
            click(4, Qt.ControlModifier)
            compare(selectedRows(), [1, 4])

            cave.removeTrip(4)
            cave.removeTrip(3)
            compare(selectedRows(), [1],
                    "rows that no longer exist must drop out of the selection")
        }

        // ── Selection shifts with row removal from the MIDDLE ────────────────

        function test_selectionFollowsRowRemovalFromMiddle() {
            setupCaveWithTrips()

            click(1, Qt.NoModifier)        // selects Trip-1 at row 1
            click(4, Qt.ControlModifier)   // also selects Trip-4 at row 4
            compare(selectedRows(), [1, 4])

            cave.removeTrip(0)             // Trip-0 gone; rows shift up by 1.

            // Trip-1 is now at row 0, Trip-4 is now at row 3.
            compare(selectedRows(), [0, 3],
                    "remaining selected rows must shift when an earlier row is removed")
        }

        // ── Appending a trip leaves the selection alone ──────────────────────

        function test_appendingRowDoesNotDisturbSelection() {
            setupCaveWithTrips()

            click(1, Qt.NoModifier)
            click(3, Qt.ControlModifier)
            const before = selectedRows()

            cave.addTrip()

            compare(selectedRows(), before,
                    "appending a row must not change which rows are selected")
        }
    }
}
