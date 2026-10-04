import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Regression test: the survey tree row's context-menu TapHandlers must not
// consume the left-click events the underlying LinkText needs to fire its
// navigation.
MainWindowTest {
    id: rootId

    LinkGenerator {
        id: linkGeneratorId
    }

    TestCase {
        name: "DelegateLinkNavigation"
        when: windowShown

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000)
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        function test_clickCaveLinkNavigatesToCavePage() {
            RootData.region.addCave()

            let caveLink = null
            tryVerify(() => {
                caveLink = ObjectFinder.findObjectByChain(mainWindow,
                    "rootId->dataMainPage->caveDelegate0->caveLink")
                return caveLink !== null
            }, 5000, "caveLink should exist")

            mouseClick(caveLink)

            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      3000, "clicking caveLink should navigate to cavePage")
        }

        function currentPageShows(objectName, node) {
            const item = RootData.pageView.currentPageItem
            return item !== null
                    && item.objectName === objectName
                    && (node === null || item.currentCave === node)
        }

        // Clicks the name link of a node or trip row on the current page's tree.
        function clickRowLink(rowName, linkName) {
            let link = null
            tryVerify(() => {
                          const page = RootData.pageView.currentPageItem
                          const row = page === null ? null : findChild(page, rowName)
                          link = row === null ? null : findChild(row, linkName)
                          return link !== null && link.visible
                      }, 5000, rowName + " should show its " + linkName)
            waitForRendering(link)
            mouseClick(link)
        }

        function crumbItems() {
            const listView = findChild(rootId.mainWindow, "linkBarListView")
            verify(listView !== null, "the link bar lists its crumbs")
            let items = []
            for (let i = 0; i < listView.count; ++i) {
                items.push(listView.itemAtIndex(i))
            }
            return items
        }

        function crumbTexts() {
            return crumbItems().map((item) => item === null ? "" : String(item.text))
        }

        function clickCrumb(text) {
            let label = null
            tryVerify(() => {
                          const items = crumbItems()
                          for (let i = 0; i < items.length; ++i) {
                              if (items[i] !== null && items[i].text === text) {
                                  label = findChild(items[i], "linkBarItemText")
                                  return label !== null
                              }
                          }
                          return false
                      }, 5000, "the link bar should show the crumb " + text)
            mouseClick(label)
        }

        // Data → Folder → Cave → Section → trip through the rows' links, then
        // back up through every crumb of the link bar.
        function test_nestedNodesNavigateDownAndBackThroughEveryCrumb() {
            const folder = RootData.region.addNode(null, SurveyNodeKind.Folder, "Kentucky")
            const cave = RootData.region.addNode(folder, SurveyNodeKind.Cave, "Side Cave")
            const section = RootData.region.addNode(cave, SurveyNodeKind.Folder, "Upper level")
            section.addTrip()
            const trip = section.trip(0)
            verify(trip !== null)

            clickRowLink("caveDelegate0", "caveLink")
            tryVerify(() => currentPageShows("cavePage", folder), 5000, "the folder's page opens")
            compare(RootData.pageSelectionModel.currentPageAddress, "Source/Data/Node=Kentucky")

            // A node page lists only its trips until C5.2 gives it a child-node
            // section, so the two node-to-node steps go by the address a child
            // row's link opens.
            RootData.pageSelectionModel.currentPageAddress = linkGeneratorId.nodeLink(cave)
            tryVerify(() => currentPageShows("cavePage", cave), 5000, "the cave's page opens")
            compare(RootData.pageSelectionModel.currentPageAddress,
                    "Source/Data/Node=Kentucky/Node=Side Cave")

            RootData.pageSelectionModel.currentPageAddress = linkGeneratorId.nodeLink(section)
            tryVerify(() => currentPageShows("cavePage", section), 5000, "the section's page opens")
            compare(RootData.pageSelectionModel.currentPageAddress,
                    "Source/Data/Node=Kentucky/Node=Side Cave/Node=Upper level")

            clickRowLink("tripDelegate0", "tripLink")
            tryVerify(() => currentPageShows("tripPage", null), 5000, "the trip's page opens")
            compare(RootData.pageView.currentPageItem.currentTrip, trip)
            compare(RootData.pageSelectionModel.currentPageAddress,
                    "Source/Data/Node=Kentucky/Node=Side Cave/Node=Upper level/Trip=" + trip.name)

            const tripName = String(trip.name)
            tryVerify(() => crumbTexts().join(" › ")
                            === "Source › All caves › Kentucky › Side Cave › Upper level › " + tripName,
                      5000, "the crumbs name each node, not its page address: " + crumbTexts().join(" › "))

            clickCrumb("Upper level")
            tryVerify(() => currentPageShows("cavePage", section), 5000, "the section crumb opens the section")

            clickCrumb("Side Cave")
            tryVerify(() => currentPageShows("cavePage", cave), 5000, "the cave crumb opens the cave")

            clickCrumb("Kentucky")
            tryVerify(() => currentPageShows("cavePage", folder), 5000, "the folder crumb opens the folder")

            clickCrumb("All caves")
            tryVerify(() => currentPageShows("dataMainPage", null), 5000, "All caves opens the Data page")
            compare(RootData.pageSelectionModel.currentPageAddress, "Source/Data")

            // The whole address, typed at once, opens the same trip.
            RootData.pageSelectionModel.currentPageAddress =
                    "Source/Data/Node=Kentucky/Node=Side Cave/Node=Upper level/Trip=" + tripName
            tryVerify(() => currentPageShows("tripPage", null), 5000, "the deep trip link opens")
            compare(RootData.pageView.currentPageItem.currentTrip, trip)
        }

        // A deep trip link opens before any page under the Data page has been
        // shown: each step down registers the page the next step needs.
        function test_deepTripLinkOpensFromTheDataPage() {
            const folder = RootData.region.addNode(null, SurveyNodeKind.Folder, "North")
            const cave = RootData.region.addNode(folder, SurveyNodeKind.Cave, "Entrance")
            const section = RootData.region.addNode(cave, SurveyNodeKind.Folder, "Lower level")
            section.addTrip()
            const trip = section.trip(0)

            RootData.pageSelectionModel.currentPageAddress = linkGeneratorId.tripLink(trip)
            tryVerify(() => currentPageShows("tripPage", null), 5000, "the deep trip link opens")
            compare(RootData.pageView.currentPageItem.currentTrip, trip)
            compare(RootData.pageSelectionModel.currentPageAddress,
                    "Source/Data/Node=North/Node=Entrance/Node=Lower level/Trip=" + trip.name)

            RootData.pageSelectionModel.back()
            tryVerify(() => currentPageShows("dataMainPage", null), 5000,
                      "back returns to the Data page, since the walk down kept no history")
        }

        // A link from before nesting, Data/Cave=X/Trip=Y, still opens the trip.
        function test_oldCaveLinkStillOpensTheTrip() {
            RootData.region.addCave()
            const cave = RootData.region.cave(0)
            cave.addTrip()
            const trip = cave.trip(0)

            RootData.pageSelectionModel.currentPageAddress =
                    "Source/Data/Cave=" + cave.name + "/Trip=" + trip.name
            tryVerify(() => currentPageShows("tripPage", null), 5000, "the old link opens the trip page")
            compare(RootData.pageView.currentPageItem.currentTrip, trip)
            compare(RootData.pageSelectionModel.currentPageAddress,
                    "Source/Data/Node=" + cave.name + "/Trip=" + trip.name)
        }
    }
}
