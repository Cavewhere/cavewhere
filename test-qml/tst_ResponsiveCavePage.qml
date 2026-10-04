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
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Node=Cave 1"
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

        // One tree serves both layouts, proxied into whichever one is
        // showing, so it is the same item at either width.
        function test_tripTreeVisibleAtWide() {
            rootId.width = 800
            waitForRendering(rootId)

            let tree = findChild(rootId, "tripTree")
            verify(tree !== null, "tripTree should exist at wide width")
            tryVerify(() => tree.visible, 5000, "tripTree should be visible at wide width")
        }

        function test_tripTreeVisibleAtNarrow() {
            rootId.width = 400
            waitForRendering(rootId)

            let tree = findChild(rootId, "tripTree")
            verify(tree !== null, "tripTree should exist at narrow width")
            tryVerify(() => tree.visible, 5000,
                      "the narrow layout shows the same tree, not a list of its own")
        }

        function test_transitionWideToNarrow() {
            rootId.width = 800
            waitForRendering(rootId)

            let tree = findChild(rootId, "tripTree")
            verify(tree !== null, "tripTree should exist at wide")
            tryVerify(() => tree.visible, 5000)

            rootId.width = 400
            waitForRendering(rootId)

            let page = findCavePage()
            verify(page.isNarrow, "Should be narrow at 400")

            compare(findChild(rootId, "tripTree"), tree,
                    "the narrow layout proxies the same tree item")
            tryVerify(() => tree.visible, 5000, "and keeps showing it")
        }

        // One header serves both layouts, as the tree does.
        function test_headerVisibleAtBothWidths() {
            rootId.width = 800
            waitForRendering(rootId)
            let chip = findChild(rootId, "nodeKindChip")
            verify(chip !== null, "the Kind chip should exist at wide width")
            tryVerify(() => chip.visible, 5000, "the header shows at wide width")

            rootId.width = 400
            waitForRendering(rootId)
            compare(findChild(rootId, "nodeKindChip"), chip, "the narrow layout proxies the same header")
            tryVerify(() => chip.visible, 5000, "and keeps showing it")
            verify(findChild(rootId, "renameNodeButton").visible, "Rename stays reachable when narrow")
        }

        // A Section is a second kind of row in the page's tree, ahead of the
        // trips, at either width.
        function test_sectionRowShowsAtBothWidths() {
            let page = findCavePage()
            let cave = page.currentNode
            let section = RootData.region.addNode(cave, SurveyNodeKind.Folder)

            for (let width of [800, 400]) {
                rootId.width = width
                waitForRendering(rootId)
                tryVerify(() => {
                              let row = findChild(rootId, "caveDelegate0")
                              return row !== null && row.visible && row.object === section
                          }, 5000, "the Section leads the tree at width " + width)
                tryVerify(() => findChild(rootId, "tripDelegate1") !== null, 5000,
                          "the cave's trips follow it at width " + width)
            }
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
