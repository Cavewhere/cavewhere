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

            rootId.width = 1200
            tryVerify(() => !cavePage.isNarrow, 5000, "flips back to wide")
            tryVerify(() => hint.visible
                            && hint.pointAtObject !== cavePage.narrowAddTripBar,
                      5000, "the hint re-anchors to the wide bar")
        }

        // The tree of the cave page: the trips of `currentCave` are its rows,
        // since the view is rooted at the cave itself.
        function tripTree(cavePage) {
            let tree = findChild(cavePage, "tripTree")
            verify(tree !== null, "the cave page must show the survey tree")
            return tree
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

            let tree = tripTree(cavePage)
            tryVerify(() => !tree.visible, 5000,
                      "the tree stays out of an empty cave's way")

            cave.addTrip()

            tryVerify(() => tree.visible, 5000,
                      "the first trip brings the tree back")
            tryVerify(() => exportButtons.visible, 5000,
                      "and makes export meaningful again")
        }

        // The cave page's tree is rooted at its own cave, so it shows that
        // cave's trips as its rows and nothing of any other cave.
        function test_treeShowsOnlyThisCavesTrips() {
            let cave = setupCaveWithTrips()

            RootData.region.addCave()
            let otherCave = RootData.region.cave(1)
            otherCave.name = "OtherCave"
            otherCave.addTrip()
            otherCave.trip(0).name = "Other-Trip"

            let cavePage = RootData.pageView.currentPageItem
            let tree = tripTree(cavePage)

            tryCompare(tree, "rowCount", 3, 5000,
                       "the cave's three trips are the tree's rows")

            let names = []
            for (let row = 0; row < tree.rowCount; row++) {
                names.push(String(tree.objectAtRow(row).name))
            }
            names.sort()
            compare(names, ["A-Trip", "B-Trip", "C-Trip"],
                    "only this cave's trips have rows: [" + names + "]")

            verify(findChild(cavePage, "caveDelegate0") === null,
                   "the cave itself is the root, so it owns no row")
        }

        // The Stations cell of a trip row names that trip's stations as the
        // trip table named them — the abbreviated range the used-station task
        // reports — and the names arrive from that task, so they are waited for.
        // Only trip rows have a cell here: the cave itself is the tree's root,
        // so the folded count belongs to the Data page's tree.
        function test_tripRowNamesItsStations() {
            let cave = setupCaveWithTrips()
            let trip = cave.trip(0)
            trip.addNewChunk()
            let chunk = trip.chunk(0)
            chunk.setData(SurveyChunk.StationNameRole, 0, "A1")
            chunk.setData(SurveyChunk.StationNameRole, 1, "A2")
            chunk.setData(SurveyChunk.ShotDistanceRole, 0, "10")
            chunk.setData(SurveyChunk.ShotCompassRole, 0, "0")
            chunk.setData(SurveyChunk.ShotClinoRole, 0, "0")

            let cavePage = RootData.pageView.currentPageItem
            let tree = tripTree(cavePage)
            tryCompare(tree, "rowCount", 3, 5000)

            // The cell is looked up again on every poll: a row the view
            // rebuilds while the task is still counting is picked up fresh.
            tryVerify(() => {
                          const cell = findChild(cavePage, "tripStations0")
                          const value = cell === null ? null : findChild(cell, "value")
                          return value !== null && value.text === "A 1-2"
                      }, 10000, "the trip row must name its stations")
        }

        // A click on a trip row's name opens that trip's page, the way the
        // trip table's name link did.
        function test_clickingATripRowOpensTheTripPage() {
            setupCaveWithTrips()

            let cavePage = RootData.pageView.currentPageItem
            let tree = tripTree(cavePage)
            tryCompare(tree, "rowCount", 3, 5000)

            let tripLink = null
            tryVerify(() => {
                          const row = findChild(cavePage, "tripDelegate0")
                          tripLink = row === null ? null : findChild(row, "tripLink")
                          return tripLink !== null && tripLink.text === "C-Trip"
                      }, 5000, "the first trip row must show its name as a link")

            mouseClick(tripLink)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "tripPage",
                      5000, "clicking a trip row must open the trip page")
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

        // Remove left the table with the sorting: a trip is deleted through
        // the tree's own context menu, which asks before it removes.
        function test_deleteTripThroughTheTreeContextMenu() {
            let cave = setupCaveWithTrips()
            let cavePage = RootData.pageView.currentPageItem
            let tree = tripTree(cavePage)
            tryCompare(tree, "rowCount", 3, 5000)

            //The row is looked up until it stands for the trip this test is
            //about: a delegate the view has yet to re-bind still draws the row
            //it last held.
            let row = null
            tryVerify(() => {
                          row = findChild(cavePage, "tripDelegate0")
                          return row !== null && row.object === cave.trip(0)
                      }, 5000, "the first trip must have a row")
            row.showContextMenu(0, 0)

            let menu = null
            tryVerify(() => {
                          menu = findChild(row, "surveyItemContextMenu")
                          return menu !== null && menu.visible
                      }, 5000, "the row must open its context menu")

            mouseClick(findChild(menu, "surveyItemDeleteMenuItem"))

            let askBox = null
            tryVerify(() => {
                          askBox = findChild(cavePage, "removeChallange")
                          return askBox !== null && askBox.visible
                      }, 5000, "Delete… must ask first")
            compare(askBox.message, "Remove <b>C-Trip</b>?")

            mouseClick(findChild(askBox, "removeButton"))

            tryCompare(cave, "tripCount", 2, 5000)
            let remaining = []
            for (let i = 0; i < cave.rowCount(); i++) {
                remaining.push(cave.trip(i).name)
            }
            compare(remaining.indexOf("C-Trip"), -1,
                    "the asked-about trip is gone: [" + remaining + "]")
        }
    }
}
