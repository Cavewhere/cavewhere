import QtQuick
import QtTest
import QtQuick.Controls as QC
import QmlTestRecorder
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    Component.onCompleted: {
        rootId.mainWindow.objectName = "mainWindow"
    }

    TestCase {
        name: "CavePageResponsive"
        when: windowShown

        function init() {
            rootId.width = 1024
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"))
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1"
            tryVerify(function() { return RootData.pageView.currentPageItem.objectName === "cavePage" })
            waitForRendering(rootId)
        }

        function cleanup() {
            rootId.width = 1200
            RootData.pageSelectionModel.currentPageAddress = "View"
            waitForRendering(rootId)
        }

        function findCavePage() {
            let page = RootData.pageView.currentPageItem
            verify(page !== null, "cavePage not found")
            verify(page.objectName === "cavePage", "Expected cavePage, got " + page.objectName)
            return page
        }

        function test_isNarrowAtSmallWidth() {
            rootId.width = 400
            waitForRendering(rootId)
            let page = findCavePage()
            verify(page.isNarrow, "Should be narrow at 400")
        }

        function test_isNotNarrowAtWideWidth() {
            rootId.width = 800
            waitForRendering(rootId)
            let page = findCavePage()
            verify(!page.isNarrow, "Should not be narrow at 800")
        }

        function test_tripTableVisibleAtWide() {
            rootId.width = 800
            waitForRendering(rootId)

            let table = findChild(rootId, "tripTableView")
            verify(table !== null, "tripTableView should exist at wide width")
            verify(table.visible, "tripTableView should be visible at wide width")
        }

        function test_tripTableHiddenAtNarrow() {
            rootId.width = 400
            waitForRendering(rootId)

            let table = findChild(rootId, "tripTableView")
            verify(table === null, "tripTableView should not exist at narrow width (Loader inactive)")
        }

        function test_transitionWideToNarrow() {
            rootId.width = 800
            waitForRendering(rootId)

            let table = findChild(rootId, "tripTableView")
            verify(table !== null, "tripTableView should exist at wide")

            rootId.width = 400
            waitForRendering(rootId)

            let page = findCavePage()
            verify(page.isNarrow, "Should be narrow at 400")

            table = findChild(rootId, "tripTableView")
            verify(table === null, "tripTableView should be gone at narrow")
        }

        function test_leadsLinkVisible() {
            let leadsLink = findChild(rootId, "leadsLink")
            verify(leadsLink !== null, "leadsLink not found")
            verify(leadsLink.visible, "leadsLink should be visible")
        }

        function test_addTripBarVisible() {
            let addBar = findChild(rootId, "addTrip")
            verify(addBar !== null, "addTrip bar not found")
            verify(addBar.visible, "addTrip bar should be visible")
        }
    }
}
