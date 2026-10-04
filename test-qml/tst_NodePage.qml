import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// The node page: one page for a Cave, a Folder, or a Section. Its header names
// the node and its kind, its stats sum the subtree, and its tree lists the
// child nodes ahead of the trips.
MainWindowTest {
    id: rootId

    TestCase {
        name: "NodePage"
        when: windowShown

        function init() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function cleanup() {
            RegionSurveyTree.cancelMove()
            RootData.project.newProject()
            rootId.width = 1200
            rootId.height = 700
        }

        function addNode(parent, kind, name) {
            const node = RootData.region.addNode(parent, kind)
            node.name = name
            return node
        }

        function addTrip(node, name, date) {
            node.addTrip()
            const trip = node.trip(node.tripCount - 1)
            trip.name = name
            trip.date = date
            return trip
        }

        function gotoNodePage(address, node) {
            RootData.pageSelectionModel.currentPageAddress = address
            tryVerify(() => {
                          const page = RootData.pageView.currentPageItem
                          return page !== null && page.objectName === "cavePage"
                              && page.currentNode === node
                      }, 5000, "the page for " + address + " should be showing")
            const page = RootData.pageView.currentPageItem
            waitForRendering(page)
            return page
        }

        // A Cave holding one Section with a trip of its own, beside one trip
        // at the cave's own level.
        function setupCaveWithSection() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            addTrip(cave, "Entrance survey", new Date(2023, 10, 12))
            const section = addNode(cave, SurveyNodeKind.Folder, "Upper level")
            addTrip(section, "Upper survey", new Date(2024, 1, 1))
            return { cave: cave, section: section }
        }

        // The tree's rows, looked up through one of them once it is drawn.
        function tree(page, firstRowName) {
            let row = null
            tryVerify(() => {
                          row = findChild(page, firstRowName)
                          return row !== null
                      }, 5000, "the node page must draw " + firstRowName)
            return row.treeView
        }

        // The objectNames of the menu entries a user sees, in order.
        function visibleEntryNames(menu) {
            const names = []
            for (let i = 0; i < menu.count; i++) {
                const item = menu.itemAt(i)
                if (item.visible) {
                    names.push(item.objectName)
                }
            }
            return names
        }

        function openAddCaret(page) {
            const addBar = findChild(page, "addTrip")
            verify(addBar !== null, "the Add bar must exist")
            const menuButton = findChild(addBar, "menuButton")
            verify(menuButton !== null, "the split button must show its caret")
            mouseClick(menuButton)

            let menu = null
            tryVerify(() => {
                          menu = findChild(page, "addTripMenu")
                          return menu !== null && menu.opened
                      }, 5000, "the caret must open the Add menu")
            return menu
        }

        function commitNewName(name) {
            tryVerify(() => rootId.shadowEditor.coreClickInput !== null, 5000,
                      "the name editor must open")
            rootId.shadowEditor.setEditorText(name)
            rootId.shadowEditor.coreClickInput.commitChanges()
        }

        function test_childSectionShowsTheSectionAndOpensIt() {
            const nodes = setupCaveWithSection()
            const page = gotoNodePage("Source/Data/Node=Side Cave", nodes.cave)

            const view = tree(page, "caveDelegate0")
            tryCompare(view, "rows", 2, 5000)
            const sectionRow = findChild(page, "caveDelegate0")
            compare(sectionRow.object, nodes.section, "the child node leads the rows")
            verify(findChild(page, "tripDelegate1") !== null, "the cave's own trip follows it")

            mouseClick(findChild(sectionRow, "caveLink"))
            tryVerify(() => RootData.pageSelectionModel.currentPageAddress
                            === "Source/Data/Node=Side Cave/Node=Upper level"
                      && RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.currentNode === nodes.section,
                      5000, "clicking the Section row opens the Section's page")
        }

        function test_childSectionStaysAwayWhileTheNodeHasNone() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            addTrip(cave, "Entrance survey", new Date(2023, 10, 12))
            const page = gotoNodePage("Source/Data/Node=Side Cave", cave)

            const view = tree(page, "tripDelegate0")
            tryCompare(view, "rows", 1, 5000)
            compare(findChild(page, "caveDelegate0"), null, "a cave without sections draws no node row")

            addNode(cave, SurveyNodeKind.Folder, "Upper level")
            tryCompare(view, "rows", 2, 5000)
            tryVerify(() => findChild(page, "caveDelegate0") !== null, 5000,
                      "the new Section shows ahead of the trips")
        }

        // A cave holding only Sections still shows its tree, so the Sections
        // can be reached from the page.
        function test_aCaveWithOnlySectionsShowsThem() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            addNode(cave, SurveyNodeKind.Folder, "Upper level")
            const page = gotoNodePage("Source/Data/Node=Side Cave", cave)

            tree(page, "caveDelegate0")
            compare(page.isEmpty, false, "a cave holding a Section has rows to show")
            tryVerify(() => findChild(page, "tripTree").visible, 5000,
                      "the tree shows the Section")
        }

        function test_addSectionFromThePage() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            const page = gotoNodePage("Source/Data/Node=Side Cave", cave)

            compare(findChild(findChild(page, "addTrip"), "addButton").text, "Add Trip")
            const menu = openAddCaret(page)
            compare(visibleEntryNames(menu),
                    ["addTripMenuItem", "addSectionMenuItem", "addExternalTripMenuItem"],
                    "inside a cave the page adds trips and sections")

            mouseClick(findChild(menu, "addSectionMenuItem"))
            tryCompare(cave, "childNodeCount", 1, 5000)
            const section = cave.childNode(0)
            compare(section.kind, SurveyNodeKind.Folder, "a Section is a Folder")
            compare(section.name, "New Section")

            commitNewName("Upper level")
            tryCompare(section, "name", "Upper level", 5000)
            compare(RootData.pageView.currentPageItem, page, "a new Section keeps the cave page in front")
        }

        // Where caves go, the page's main action and menu add caves and
        // folders, and the survey-file entry for a trip stays away.
        function test_folderPageAddsCaves() {
            const folder = addNode(null, SurveyNodeKind.Folder, "Kentucky")
            const page = gotoNodePage("Source/Data/Node=Kentucky", folder)

            const addButton = findChild(findChild(page, "addTrip"), "addButton")
            compare(addButton.text, "Add Cave")

            const menu = openAddCaret(page)
            compare(visibleEntryNames(menu), ["addCaveMenuItem", "addFolderMenuItem"],
                    "a Folder outside every cave takes caves and folders")
            menu.close()
            tryVerify(() => !menu.opened, 5000)

            mouseClick(addButton)
            tryCompare(folder, "childNodeCount", 1, 5000)
            const cave = folder.childNode(0)
            compare(cave.kind, SurveyNodeKind.Cave)
            gotoNodePage("Source/Data/Node=Kentucky/Node=New Cave", cave)
        }

        function test_emptyFolderHintNamesCaves() {
            const folder = addNode(null, SurveyNodeKind.Folder, "Kentucky")
            const page = gotoNodePage("Source/Data/Node=Kentucky", folder)

            const hint = findChild(page, "noTripsHint")
            tryVerify(() => hint.visible, 5000, "an empty Folder explains itself")
            verify(hint.text.indexOf("add a cave") >= 0, "the hint names what a Folder takes")

            addNode(folder, SurveyNodeKind.Cave, "Fisher Ridge")
            tryVerify(() => !hint.visible, 5000, "the first cave retires the hint")
        }

        // §3.1: fix stations exist on every native node, so a Folder's page
        // shows the row and opens the same fix-station page.
        function test_fixStationRowOnAFolderNode() {
            const folder = addNode(null, SurveyNodeKind.Folder, "Kentucky")
            const page = gotoNodePage("Source/Data/Node=Kentucky", folder)

            const link = findChild(page, "fixStationsLink")
            verify(link !== null && link.visible, "a Folder page shows its fix stations")
            compare(link.text, "0")

            mouseClick(link)
            tryVerify(() => {
                          const current = RootData.pageView.currentPageItem
                          return current !== null && current.objectName === "fixStationPage"
                      }, 5000, "the link opens the fix-station page")
            compare(RootData.pageView.currentPageItem.cave, folder, "the page edits the Folder's fixes")
        }

        function test_headerNamesTheKindAndThePickerRelabels() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            const page = gotoNodePage("Source/Data/Node=Side Cave", cave)

            const chip = findChild(page, "nodeKindChip")
            const icon = findChild(page, "nodeKindIcon")
            compare(chip.text, "Cave")
            verify(chip.pickable, "a native node's chip is the picker")
            verify(String(icon.source).indexOf("caveKind") >= 0, "a Cave draws the cave icon")

            mouseClick(chip)
            const kindMenu = findChild(chip, "kindPickerMenu")
            tryVerify(() => kindMenu.opened, 5000, "the chip opens the picker")
            mouseClick(findChild(kindMenu, "kindPickerFolder"))

            tryCompare(cave, "kind", SurveyNodeKind.Folder, 5000)
            tryCompare(chip, "text", "Folder", 5000)
            verify(String(icon.source).indexOf("folder") >= 0, "a Folder draws the folder icon")
            verify(findChild(page, "fixStationsLink").visible, "relabeling keeps the fix-station row")
        }

        function test_renameButtonOpensTheNameEditor() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            const page = gotoNodePage("Source/Data/Node=Side Cave", cave)

            mouseClick(findChild(page, "renameNodeButton"))
            commitNewName("Fisher Ridge")
            tryCompare(cave, "name", "Fisher Ridge", 5000)
            tryCompare(findChild(page, "nodeNameText"), "text", "Fisher Ridge", 5000)
        }

        function test_statsCountEveryTripBelowTheNode() {
            const nodes = setupCaveWithSection()
            const page = gotoNodePage("Source/Data/Node=Side Cave", nodes.cave)

            tryCompare(findChild(page, "tripCountValue"), "text", "2", 5000)
            tryCompare(findChild(page, "lastSurveyValue"), "text", "2024-02-01", 5000)

            addTrip(nodes.section, "Later survey", new Date(2025, 4, 10))
            tryCompare(findChild(page, "tripCountValue"), "text", "3", 5000)
            tryCompare(findChild(page, "lastSurveyValue"), "text", "2025-05-10", 5000)
        }

        // Move to… on the header opens the Data page with the move armed,
        // where the whole tree picks the destination; the top level is offered
        // as a button, since the region's root owns no row.
        function test_moveToFromTheHeaderArmsTheDataPage() {
            const nodes = setupCaveWithSection()
            const page = gotoNodePage("Source/Data/Node=Side Cave/Node=Upper level", nodes.section)

            const moveButton = findChild(page, "moveNodeButton")
            verify(moveButton !== null && moveButton.visible, "a native node offers Move to…")
            mouseClick(moveButton)

            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "Move to… opens the Data page")
            verify(RegionSurveyTree.moveSubject === nodes.section, "the move carries the page's node")

            const dataPage = RootData.pageView.currentPageItem
            const banner = findChild(dataPage, "surveyMoveBanner")
            tryVerify(() => banner.visible, 5000, "the Data page shows the armed move")
            compare(findChild(banner, "surveyMoveBannerLabel").text,
                    "Click where to move Upper level — Esc cancels")

            const topLevel = findChild(banner, "surveyMoveTopLevelButton")
            tryVerify(() => topLevel.visible, 5000, "the top level takes a section")
            waitForRendering(banner)
            mouseClick(topLevel)

            tryCompare(RootData.region, "caveCount", 2, 5000)
            compare(nodes.cave.childNodeCount, 0)
            verify(!RegionSurveyTree.moveActive, "landing the move disarms it")

            RootData.undoStack.undo()
            tryCompare(nodes.section, "parentNode", nodes.cave, 5000)
        }

        // A top-level cave's header offers the move too: a folder can take it.
        function test_moveToShowsOnATopLevelCaveHeader() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            const page = gotoNodePage("Source/Data/Node=Side Cave", cave)
            tryVerify(() => findChild(page, "moveNodeButton").visible, 5000,
                      "a top-level cave can move into a folder")
        }
    }
}
