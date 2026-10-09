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

        // One header and one Kind chip serve both layouts, as the tree does.
        function test_headerVisibleAtBothWidths() {
            rootId.width = 800
            waitForRendering(rootId)
            let chip = findChild(rootId, "nodeKindChip")
            verify(chip !== null, "the Kind chip should exist at wide width")
            tryVerify(() => chip.visible, 5000, "the chip shows at wide width")
            const name = findChild(rootId, "nodeNameText")
            tryVerify(() => name.visible, 5000, "the header shows at wide width")

            rootId.width = 400
            waitForRendering(rootId)
            compare(findChild(rootId, "nodeKindChip"), chip, "the narrow layout proxies the same chip")
            tryVerify(() => chip.visible, 5000, "and keeps showing it")
            compare(findChild(rootId, "nodeNameText"), name, "the narrow layout proxies the same header")
            tryVerify(() => name.visible, 5000, "and keeps showing it")
        }

        // The right edge of what \a item paints, in window coordinates: its
        // own box, or wider where its text runs past it.
        function paintedRight(item) {
            let right = item.width
            for (let i = 0; i < item.children.length; i++) {
                const child = item.children[i]
                if (child.visible && child.contentWidth !== undefined) {
                    right = Math.max(right, child.x + child.contentWidth)
                }
            }
            return item.mapToItem(null, right, 0).x
        }

        // In the wide layout the stats column sits left of the tree, a long
        // node name included: the name wraps inside the column rather than
        // running under the tree.
        function test_wideSidebarStaysLeftOfTheTable() {
            const page = findCavePage()
            page.currentNode.name = "Fisher Ridge Cave System Upper Entrance Complex North"

            for (let width of [1024, 800, 650]) {
                rootId.width = width
                waitForRendering(rootId)
                verify(!page.isNarrow, "the wide layout shows at width " + width)

                const tree = findChild(rootId, "tripTree")
                const statsBox = findChild(rootId, "nodeStatsBox")
                const name = findChild(rootId, "nodeNameText")
                tryVerify(() => tree.visible && statsBox.visible && name.visible, 5000)
                const sidebar = statsBox.parent
                tryVerify(() => sidebar.mapToItem(null, sidebar.width, 0).x <= tree.mapToItem(null, 0, 0).x,
                          5000, "the sidebar ends before the table at width " + width)
                const tableLeft = tree.mapToItem(null, 0, 0).x

                verify(statsBox.mapToItem(null, statsBox.width, 0).x <= tableLeft,
                       "the stats end before the table at width " + width)
                verify(statsBox.implicitWidth <= statsBox.width,
                       "the stats fit their box at width " + width)
                tryVerify(() => paintedRight(name) <= tableLeft, 5000,
                          "the name ends before the table at width " + width
                          + "; it reaches " + paintedRight(name) + ", the table starts at " + tableLeft)
            }
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
