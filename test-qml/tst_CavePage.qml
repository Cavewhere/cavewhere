import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    TestCase {
        name: "CavePage"
        when: windowShown

        function init() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function cleanup() {
            RootData.project.newProject()
        }

        function setupCaveWithTrips() {
            RootData.region.addCave()
            let cave = RootData.region.cave(0)
            cave.name = "TestCave"

            cave.addTrip()
            cave.trip(0).name = "C-Trip"

            cave.addTrip()
            cave.trip(1).name = "A-Trip"

            cave.addTrip()
            cave.trip(2).name = "B-Trip"

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=TestCave"
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "cavePage")
            waitForRendering(rootId)

            return cave
        }

        // Reproduces issue #294: sorted proxy index was passed directly to
        // cave.removeTrip() instead of mapping back to the source index.
        function test_deleteTripAfterSortDeletesCorrectTrip() {
            let cave = setupCaveWithTrips()

            compare(cave.rowCount(), 3)
            compare(cave.trip(0).name, "C-Trip")
            compare(cave.trip(1).name, "A-Trip")
            compare(cave.trip(2).name, "B-Trip")

            let cavePage = RootData.pageView.currentPageItem
            verify(cavePage !== null)

            let tableView = findChild(cavePage, "tripTableView")
            verify(tableView !== null, "Trip table must exist")

            let sortModel = tableView.model
            verify(sortModel !== null, "SortFilterProxyModel must exist")

            sortModel.sortRole = CavePageModel.TripNameRole
            sortModel.sort(Qt.AscendingOrder)
            waitForRendering(rootId)

            // Sorted: A-Trip(proxy 0), B-Trip(proxy 1), C-Trip(proxy 2)
            compare(sortModel.count, 3)
            let proxyIdx0 = sortModel.index(0, 0)
            compare(sortModel.data(proxyIdx0, CavePageModel.TripNameRole), "A-Trip")

            let sourceIdx = sortModel.mapToSource(proxyIdx0)
            compare(sourceIdx.row, 1, "Proxy 0 (A-Trip) should map to source index 1")

            let removeAskBox = findChild(cavePage, "removeChallange")
            verify(removeAskBox !== null)

            removeAskBox.indexToRemove = 0
            removeAskBox.removeName = "A-Trip"
            removeAskBox.show()
            tryVerify(() => removeAskBox.visible)

            let removeButton = findChild(removeAskBox, "removeButton")
            mouseClick(removeButton)
            tryVerify(() => !removeAskBox.visible)

            compare(cave.rowCount(), 2, "Should have 2 trips after removal")

            let remainingNames = []
            for (let i = 0; i < cave.rowCount(); i++) {
                remainingNames.push(cave.trip(i).name)
            }

            verify(remainingNames.indexOf("A-Trip") === -1,
                   "A-Trip should have been removed but remaining trips are: [" + remainingNames + "]")
            verify(remainingNames.indexOf("C-Trip") !== -1,
                   "C-Trip should still exist but remaining trips are: [" + remainingNames + "]")
            verify(remainingNames.indexOf("B-Trip") !== -1,
                   "B-Trip should still exist but remaining trips are: [" + remainingNames + "]")
        }
    }
}
