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

        //! The note carrying the leads, so a caller can add and remove more
        function setupCaveWithLeads(caveName, leadCount) {
            RootData.region.addCave()
            let cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = caveName

            cave.addTrip()
            let trip = cave.trip(0)
            trip.name = caveName + "-Trip"

            let note = TestHelper.addNoteWithScrap(trip, caveName + "-Note")
            verify(note !== null, "note with a scrap should be added to " + caveName)

            for (let i = 0; i < leadCount; ++i) {
                TestHelper.addScrapLead(note, 0, Qt.point(i, i), Qt.size(1, 1),
                                        caveName + " lead " + i)
            }

            return note
        }

        function gotoCavePage(caveName) {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + caveName
            tryVerify(() => {
                          let page = RootData.pageView.currentPageItem
                          return page !== null && page.objectName === "cavePage"
                              && page.currentCave !== null && page.currentCave.name === caveName
                      }, 5000, "cave page should be showing " + caveName)
            return RootData.pageView.currentPageItem
        }

        function gotoCaveLeadsLink(caveName) {
            let leadsLink = findChild(gotoCavePage(caveName), "leadsLink")
            verify(leadsLink !== null, "leadsLink should exist on the cave page")
            return leadsLink
        }

        // Regression test for issue #657: the page view keeps one CavePage item
        // and reassigns currentCave, so the "Leads:" count has to follow the
        // cave the page is showing rather than the one it was created with.
        function test_leadCountFollowsCurrentCave() {
            setupCaveWithLeads("LeadCaveA", 3)
            setupCaveWithLeads("LeadCaveB", 1)

            let leadsLink = gotoCaveLeadsLink("LeadCaveA")
            tryCompare(leadsLink, "text", "3", 5000,
                       "cave page should show LeadCaveA's 3 leads")

            gotoCavePage("LeadCaveB")
            tryCompare(leadsLink, "text", "1", 5000,
                       "cave page should show LeadCaveB's 1 lead, not the count it was showing for LeadCaveA")

            gotoCavePage("LeadCaveA")
            tryCompare(leadsLink, "text", "3", 5000,
                       "cave page should show LeadCaveA's 3 leads again")
        }

        // The count also has to follow leads coming and going in the cave the
        // page is already showing, not just a switch to another cave.
        function test_leadCountFollowsLeadEdits() {
            let note = setupCaveWithLeads("LeadEditCave", 2)

            let leadsLink = gotoCaveLeadsLink("LeadEditCave")
            tryCompare(leadsLink, "text", "2", 5000,
                       "cave page should show the 2 leads it started with")

            verify(TestHelper.addScrapLead(note, 0, Qt.point(9, 9), Qt.size(1, 1), "added lead"))
            tryCompare(leadsLink, "text", "3", 5000,
                       "adding a lead should raise the count")

            verify(TestHelper.removeScrapLead(note, 0, 0))
            tryCompare(leadsLink, "text", "2", 5000,
                       "removing a lead should lower the count")
        }

        // Give each test its own cave name: a second page registered under the
        // same name in one run crashes cwPage teardown at exit.
        function setupCaveWithTrips(caveName = "TestCave") {
            RootData.region.addCave()
            let cave = RootData.region.cave(0)
            cave.name = caveName

            cave.addTrip()
            cave.trip(0).name = "C-Trip"

            cave.addTrip()
            cave.trip(1).name = "A-Trip"

            cave.addTrip()
            cave.trip(2).name = "B-Trip"

            gotoCavePage(caveName)
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

        function showTripRemoveAskBox(cavePage, removeName) {
            let removeAskBox = findChild(cavePage, "removeChallange")
            verify(removeAskBox !== null, "the cave page should declare a remove ask box")

            removeAskBox.indexToRemove = 0
            removeAskBox.removeName = removeName
            removeAskBox.show()
            tryVerify(() => removeAskBox.visible, 5000, "show() should open the remove ask box")
            return removeAskBox
        }

        // Issue #720: Escape dismisses the ask box and removes nothing.
        function test_escapeClosesRemoveAskBox() {
            let cave = setupCaveWithTrips("EscapeCave")
            let cavePage = RootData.pageView.currentPageItem
            let removeAskBox = showTripRemoveAskBox(cavePage, "C-Trip")

            keyClick(Qt.Key_Escape)
            tryVerify(() => !removeAskBox.visible, 5000, "Escape should close the remove ask box")
            compare(cave.rowCount(), 3, "closing the box with Escape should keep every trip")
        }

        // Issue #720: a press outside the ask box closes it and removes nothing.
        function test_pressOutsideClosesRemoveAskBox() {
            let cave = setupCaveWithTrips("OutsidePressCave")
            let cavePage = RootData.pageView.currentPageItem
            let removeAskBox = showTripRemoveAskBox(cavePage, "C-Trip")

            // show() leaves the box at the page's top-left; the trip table is
            // only as tall as its rows, so the bottom-right corner is empty page.
            const outsideInset = 10
            mouseClick(cavePage, cavePage.width - outsideInset, cavePage.height - outsideInset)
            tryVerify(() => !removeAskBox.visible, 5000, "a press outside should close the remove ask box")
            compare(cave.rowCount(), 3, "closing the box with an outside press should keep every trip")
        }

        // Issue #720: the removed name shows as plain text, so markup characters stay literal.
        function test_removeNameRendersAsPlainText() {
            setupCaveWithTrips("PlainTextCave")
            const markupName = "<i>A</i> & B"
            let removeAskBox = showTripRemoveAskBox(RootData.pageView.currentPageItem, markupName)

            let nameLabel = findChild(removeAskBox, "removeNameLabel")
            verify(nameLabel !== null, "the remove ask box should show the name in its own label")
            compare(nameLabel.text, markupName)
            compare(nameLabel.textFormat, Text.PlainText)
            verify(nameLabel.visible, "the name label should show a non-empty name")

            keyClick(Qt.Key_Escape)
            tryVerify(() => !removeAskBox.visible, 5000, "Escape should close the remove ask box")
        }

        // Issue #720: a box without a name shows only its buttons.
        function test_emptyRemoveNameHidesNameLabel() {
            setupCaveWithTrips("EmptyNameCave")
            let removeAskBox = showTripRemoveAskBox(RootData.pageView.currentPageItem, "")

            let nameLabel = findChild(removeAskBox, "removeNameLabel")
            verify(nameLabel !== null, "the remove ask box should declare its name label")
            verify(!nameLabel.visible, "an empty name should hide the name label")

            keyClick(Qt.Key_Escape)
            tryVerify(() => !removeAskBox.visible, 5000, "Escape should close the remove ask box")
        }
    }
}
