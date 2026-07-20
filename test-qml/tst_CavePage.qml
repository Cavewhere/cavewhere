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

            // Tests that resize the window or scale the font must not leak
            // that into whatever runs next.
            rootId.width = 1200
            rootId.height = 700
            RootData.settings.fontSettings.fontBaseSize = 16
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

        function setupEmptyCave() {
            RootData.region.addCave()
            let cave = RootData.region.cave(0)
            cave.name = "EmptyCave"

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=EmptyCave"
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "cavePage")
            waitForRendering(rootId)

            return cave
        }

        function test_noTripsHelpShowsOnlyWhileCaveIsEmpty() {
            let cave = setupEmptyCave()
            let cavePage = RootData.pageView.currentPageItem

            let help = findChild(cavePage, "noTripsHint")
            verify(help !== null, "the empty state must exist")
            tryVerify(() => help.visible, 5000, "a cave with no trips explains itself")

            cave.addTrip()
            tryVerify(() => !help.visible, 5000, "the first trip retires the help")

            cave.removeTrip(0)
            tryVerify(() => help.visible, 5000,
                      "deleting the last trip brings the help back")
        }

        // The arrow used to sit wherever the first layout pass left it and
        // only snap onto the button once something forced a reposition.
        function test_noTripsHintTracksAddTripAcrossLayout() {
            rootId.width = 1024
            setupEmptyCave()
            let cavePage = RootData.pageView.currentPageItem

            let hint = findChild(cavePage, "noTripsHint")
            tryVerify(() => hint.visible, 5000, "the hint shows")
            let addTrip = findChild(cavePage, "addTrip")
            verify(addTrip !== null, "the Add Trip bar must exist")

            // triangleOffset is 0, so the arrow tip is the box's origin.
            function arrowIsOnAddTrip() {
                let tip = hint.mapToItem(cavePage, 0, 0)
                let anchor = addTrip.mapToItem(cavePage,
                                               addTrip.width / 2.0,
                                               addTrip.height)
                return Math.abs(tip.x - anchor.x) < 2
                        && Math.abs(tip.y - anchor.y) < 2
            }

            tryVerify(arrowIsOnAddTrip, 5000,
                      "the arrow lands on Add Trip with no resize")

            rootId.width = 1400
            tryVerify(arrowIsOnAddTrip, 5000, "and follows the button on resize")
        }

        // The body opens rightward from an arrow anchored at the Add Trip
        // bar's centre, so a cap measured against the page width alone let
        // the right edge run off the narrowest wide window at a large font.
        function test_noTripsHintStaysInsideTheWindow() {
            setupEmptyCave()
            let cavePage = RootData.pageView.currentPageItem

            let hint = findChild(cavePage, "noTripsHint")
            tryVerify(() => hint.visible, 5000, "the hint shows")

            let label = findChild(hint, "helpText")
            verify(label !== null, "the hint's label must exist")

            // 700 is the narrowest window that still lays out wide; the
            // overflow only appeared at the enlarged font sizes.
            let baseSizes = [16, 20, 24, 28]
            let widths = [1400, 900, 760, 700]

            for (let b = 0; b < baseSizes.length; b++) {
                RootData.settings.fontSettings.fontBaseSize = baseSizes[b]

                for (let i = 0; i < widths.length; i++) {
                    rootId.width = widths[i]
                    waitForRendering(cavePage)

                    let where = "at base " + baseSizes[b]
                            + ", page " + cavePage.width
                    tryVerify(() => label.mapToItem(cavePage, 0, 0).x >= 0,
                              5000, "the hint clears the left edge " + where)
                    tryVerify(() => label.mapToItem(cavePage, label.width, 0).x
                                    <= cavePage.width,
                              5000, "the hint clears the right edge " + where)
                }
            }

            RootData.settings.fontSettings.fontBaseSize = 16
        }

        // The hint is a page-level sibling of the scrolling area, so it
        // followed the Add Trip bar under the clip edge and drew itself
        // over the chrome above the page.
        function test_noTripsHintDoesNotEscapeWhileScrolling() {
            rootId.width = 1024
            rootId.height = 150
            setupEmptyCave()
            let cavePage = RootData.pageView.currentPageItem

            let hint = findChild(cavePage, "noTripsHint")
            tryVerify(() => hint.visible, 5000, "the hint shows")

            let flickable = findChild(cavePage, "cavePageVerticalScrollBar").parent
            verify(flickable !== null, "the page flickable must exist")
            verify(flickable.contentHeight > flickable.height,
                   "this window must be short enough to scroll")

            verify(hint.anchorInsideClip, "the box draws before scrolling")

            flickable.contentY = flickable.contentHeight - flickable.height
            waitForRendering(cavePage)

            // Scrolled to the bottom the Add Trip bar has left the viewport,
            // and nothing clips the hint, so it must stop drawing itself.
            tryVerify(() => !hint.anchorInsideClip, 5000,
                      "the hint retires once its anchor scrolls away")

            flickable.contentY = 0
            waitForRendering(cavePage)
            tryVerify(() => hint.anchorInsideClip, 5000,
                      "and comes back when the bar scrolls into view")
        }

        // The narrow layout builds its own Add Trip bar inside a Component,
        // so it publishes itself up to the page for the hint to point at.
        // Nothing else covers that handoff.
        function test_noTripsHintFollowsTheNarrowAddTripBar() {
            rootId.width = 500
            setupEmptyCave()
            let cavePage = RootData.pageView.currentPageItem
            verify(cavePage.isNarrow, "500 must lay out narrow")

            let hint = findChild(cavePage, "noTripsHint")
            tryVerify(() => cavePage.narrowAddTripBar !== null, 5000,
                      "the narrow bar publishes itself on completion")
            tryVerify(() => hint.visible, 5000, "the hint shows when narrow")
            compare(hint.pointAtObject, cavePage.narrowAddTripBar,
                    "and points at the narrow bar, not the wide one")

            let sortControls = findChild(cavePage, "narrowSortControls")
            verify(sortControls !== null, "the narrow sort row must exist")
            verify(!sortControls.visible, "nothing to sort on an empty cave")

            rootId.width = 1200
            tryVerify(() => !cavePage.isNarrow, 5000, "flips back to wide")
            tryVerify(() => hint.visible
                            && hint.pointAtObject !== cavePage.narrowAddTripBar,
                      5000, "the hint re-anchors to the wide bar")
        }

        // The hint stands alone on an empty cave: an empty table and an
        // Export button with nothing to export would only crowd it.
        function test_emptyCaveHidesTableAndExport() {
            let cave = setupEmptyCave()
            let cavePage = RootData.pageView.currentPageItem

            let exportButtons = findChild(cavePage, "exportImportButtons")
            verify(exportButtons !== null, "the export bar must exist")
            tryVerify(() => !exportButtons.visible, 5000,
                      "nothing to export from an empty cave")
            verify(findChild(cavePage, "tripTableView") === null,
                   "the trip table must not be loaded for an empty cave")

            cave.addTrip()

            tryVerify(() => findChild(cavePage, "tripTableView") !== null, 5000,
                      "the first trip brings the table back")
            tryVerify(() => exportButtons.visible, 5000,
                      "and makes export meaningful again")
        }

        // The file-backed path adds a trip the same way, so the help must
        // retire on it too - not just on the plain Add Trip button.
        function test_noTripsHelpHidesAfterAddingTripFromSurveyFile() {
            let cave = setupEmptyCave()
            let cavePage = RootData.pageView.currentPageItem

            let help = findChild(cavePage, "noTripsHint")
            tryVerify(() => help.visible, 5000, "starts on an empty cave")

            // By objectName, not itemAt(0): a new menu entry ahead of this
            // one would otherwise silently retarget the test.
            let menuItem = findChild(cavePage, "addExternalTripMenuItem")
            verify(menuItem !== null, "the survey-file menu item must exist")
            menuItem.triggered()

            let dialog = findChild(cavePage, "attachExternalCenterlineDialog")
            verify(dialog !== null, "the survey-file dialog must exist")
            let pathField = findChild(dialog, "sourcePathField")
            tryVerify(() => pathField.visible, 5000, "dialog opens")
            pathField.text = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_simple.svx")

            let attachButton = findChild(dialog, "attachButton")
            tryVerify(() => attachButton.enabled, 10000, "preview scan lands")
            waitForRendering(cavePage)
            mouseClick(attachButton)

            tryVerify(() => cave.rowCount() === 1, 10000, "the attach lands a trip")
            tryVerify(() => !help.visible, 5000, "a file-backed trip retires the help")
        }

        // The sort arrow lived on the header delegate, so tearing the table
        // down for an empty cave lost it while the proxy kept on sorting -
        // the rows came back sorted with no column saying so.
        function test_sortIndicatorSurvivesAnEmptyCave() {
            let cave = setupCaveWithTrips()
            let cavePage = RootData.pageView.currentPageItem

            let nameHeader = findChild(cavePage, "headerColumn-Name")
            verify(nameHeader !== null, "the Name header must exist")

            mouseClick(nameHeader)
            tryVerify(() => nameHeader.sortMode
                            !== TableStaticHeaderColumn.Sort.None,
                      5000, "clicking the header sorts by name")

            let sortedRole = findChild(cavePage, "tripTableView").model.sortRole

            while (cave.rowCount() > 0) {
                cave.removeTrip(0)
            }
            tryVerify(() => findChild(cavePage, "tripTableView") === null,
                      5000, "the empty cave tears the table down")

            cave.addTrip()
            tryVerify(() => findChild(cavePage, "tripTableView") !== null,
                      5000, "the table comes back with the first trip")

            compare(findChild(cavePage, "tripTableView").model.sortRole,
                    sortedRole, "the proxy is still sorting by that role")

            let restored = findChild(cavePage, "headerColumn-Name")
            verify(restored !== null, "the Name header comes back")
            verify(restored.sortMode !== TableStaticHeaderColumn.Sort.None,
                   "the header must still show which column is sorted")
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
