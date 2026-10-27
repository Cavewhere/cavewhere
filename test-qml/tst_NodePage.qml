import QtQuick
import QtCore as QtCore
import QtTest
import cavewherelib
import cw.TestLib

// The node page: one page for a Cave, a Folder, or a Section. Its header names
// the node and its kind, its stats sum the subtree, and its tree lists the
// child nodes ahead of the trips.
MainWindowTest {
    id: rootId

    readonly property list<string> treeColumnWidthKeys: ["kindColumnWidth", "tripsColumnWidth",
        "stationsColumnWidth", "lengthColumnWidth", "depthColumnWidth", "dateColumnWidth",
        "declColumnWidth"]

    // The survey tree's dragged column widths; cleanup() puts them back.
    QtCore.Settings {
        id: treeColumnSettingsId
        category: "SurveyTreeView"
    }

    ExternalCenterlineTestCase {
        name: "NodePage"
        when: windowShown

        // Wide enough that the Name column has room to give a dragged column.
        readonly property int wideWindowWidth: 1600
        readonly property int headerDragDistance: 40

        // Enough trips that a tree drawing every row is many viewports tall.
        readonly property int manyTrips: 300
        readonly property int tallWindowHeight: 900
        readonly property int shortWindowHeight: 600
        readonly property int tinyWindowHeight: 200
        // A partly shown row at each edge plus TableView's buffer rows.
        readonly property int extraLiveRows: 4
        readonly property int wheelSteps: 3
        readonly property int wheelNotch: 120

        function init() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function cleanup() {
            RegionSurveyTree.cancelMove()
            RootData.project.newProject()
            rootId.width = 1200
            rootId.height = 700

            for (const key of rootId.treeColumnWidthKeys) {
                treeColumnSettingsId.setValue(key, 0)
            }
            treeColumnSettingsId.sync()
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

        // The expand/collapse button the node page's tree shares with the Data page.
        function expandToggle(page) {
            const button = findChild(findChild(page, "tripTree"), "surveyTreeExpandToggle")
            verify(button !== null, "the node page's tree holds the expand/collapse button")
            return button
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

        // The wide node page's tree scrolls itself, and a drag on its header
        // boundary resizes the column, with the rows' cells following the
        // header.
        function test_aDraggedHeaderBoundaryResizesTheNodeTreeColumn() {
            rootId.width = wideWindowWidth
            const nodes = setupCaveWithSection()
            const page = gotoNodePage("Source/Data/Node=Side Cave", nodes.cave)
            const view = tree(page, "caveDelegate0")
            tryCompare(view, "rows", 2, 5000)
            const treeHost = findChild(page, "tripTree")
            verify(view.interactive, "the wide node page's tree scrolls itself")

            const kindHeader = findChild(treeHost, "surveyTreeHeaderCell" + SurveyTreeModel.Kind)
            const nameHeader = findChild(treeHost, "surveyTreeHeaderCell" + SurveyTreeModel.Name)
            verify(kindHeader !== null && nameHeader !== null, "the node tree has Name and Kind headers")
            waitForRendering(page)
            const startKind = kindHeader.width
            const drag = headerDragDistance
            verify(nameHeader.width - Theme.treeNameColumnMinimumWidth > drag,
                   "the Name column (" + nameHeader.width + ") has room to give " + drag)

            const grip = findChild(kindHeader, "surveyTreeResizeGrip" + SurveyTreeModel.Kind)
            verify(grip !== null, "the node tree's Kind header has a resize grip")
            const start = grip.mapToItem(page, grip.width / 2, grip.height / 2)
            mouseDrag(page, start.x, start.y, -drag, 0)

            tryVerify(() => Math.abs(kindHeader.width - (startKind + drag)) <= 2, 5000,
                      "the Kind header widens by the drag, from " + startKind
                      + " to about " + (startKind + drag) + ", got " + kindHeader.width)
            const draggedWidth = kindHeader.width
            for (let row = 0; row < 2; row++) {
                tryVerify(() => {
                              const cell = view.itemAtCell(Qt.point(SurveyTreeModel.Kind, row))
                              return cell !== null && cell.width === draggedWidth
                          }, 5000, "row " + row + "'s Kind cell follows its column to " + draggedWidth)
            }
        }

        function pageFlickable(page) {
            const bar = findChild(page, "cavePageVerticalScrollBar")
            verify(bar !== null, "the wide node page has its scrollbar")
            return bar.parent
        }

        // A cave of many trips draws only the rows in its window, and a wheel
        // over the tree scrolls the tree.
        function test_aNodeWithManyTripsDrawsOnlyTheRowsInView() {
            rootId.width = wideWindowWidth
            rootId.height = tallWindowHeight
            const cave = addNode(null, SurveyNodeKind.Cave, "Long Cave")
            for (let i = 0; i < manyTrips; i++) {
                cave.addTrip()
            }
            const page = gotoNodePage("Source/Data/Node=Long Cave", cave)
            const view = tree(page, "tripDelegate0")
            tryCompare(view, "rows", manyTrips, 5000)
            waitForRendering(page)

            const liveRows = rootId.collectByName(page, "rowContent").length
            const rowsInView = Math.ceil(view.height / Theme.treeRowHeight) + extraLiveRows
            verify(liveRows <= rowsInView,
                   "the tree draws the rows its " + view.height + " px window shows (at most "
                   + rowsInView + "), not all " + manyTrips + ": it drew " + liveRows)
            verify(liveRows < manyTrips / 4,
                   "the tree draws a fraction of its " + manyTrips + " rows, drew " + liveRows)
            verify(view.contentHeight > view.height,
                   "the tree's rows (" + view.contentHeight + " px) outgrow its window ("
                   + view.height + " px)")

            const center = view.mapToItem(page, view.width / 2, view.height / 2)
            for (let step = 0; step < wheelSteps; step++) {
                mouseWheel(page, center.x, center.y, 0, -wheelNotch)
            }
            tryVerify(() => view.contentY > 0, 5000,
                      "a wheel over the tree scrolls the tree, contentY " + view.contentY)
        }

        // The survey tree's own vertical bar, beside the rows rather than the
        // page's.
        function treeScrollBar(page) {
            const bar = findChild(page, "surveyTreeVerticalScrollBar")
            verify(bar !== null, "the node page's tree has its own vertical scrollbar")
            return bar
        }

        // The right edge of row 0's last cell, in the view's coordinates.
        function declCellRight(view) {
            const cell = view.itemAtCell(Qt.point(SurveyTreeModel.Decl, 0))
            verify(cell !== null, "row 0 draws its Decl cell")
            return cell.mapToItem(view, cell.width, 0).x
        }

        // A long trip list on the wide node page scrolls inside the tree, so
        // the tree carries the bar that shows where the rows stand, and the
        // bar covers none of the last column.
        function test_theWideNodeTreeShowsItsScrollBar() {
            rootId.width = wideWindowWidth
            rootId.height = tallWindowHeight
            const cave = addNode(null, SurveyNodeKind.Cave, "Long Cave")
            for (let i = 0; i < manyTrips; i++) {
                cave.addTrip()
            }
            const page = gotoNodePage("Source/Data/Node=Long Cave", cave)
            const view = tree(page, "tripDelegate0")
            tryCompare(view, "rows", manyTrips, 5000)
            waitForRendering(page)

            const bar = treeScrollBar(page)
            tryVerify(() => bar.size < 1, 5000,
                      "the bar's handle shows part of the rows, size " + bar.size)
            tryVerify(() => declCellRight(view) <= bar.x + 1, 5000,
                      "the Decl column ends at " + declCellRight(view)
                      + ", left of the bar at " + bar.x)

            const startPosition = bar.position
            const center = view.mapToItem(page, view.width / 2, view.height / 2)
            for (let step = 0; step < wheelSteps; step++) {
                mouseWheel(page, center.x, center.y, 0, -wheelNotch)
            }
            tryVerify(() => bar.position > startPosition, 5000,
                      "scrolling the tree moves the bar from " + startPosition
                      + ", got " + bar.position)
        }

        // A cave whose trips fit the tree has nothing to scroll, so the bar
        // shows no handle and the Name column keeps the whole remainder.
        function test_aShortNodeTreeShowsNoScrollBarHandle() {
            rootId.width = wideWindowWidth
            rootId.height = tallWindowHeight
            const cave = addNode(null, SurveyNodeKind.Cave, "Short Cave")
            addTrip(cave, "Only survey", new Date(2024, 1, 1))
            const page = gotoNodePage("Source/Data/Node=Short Cave", cave)
            const view = tree(page, "tripDelegate0")
            tryCompare(view, "rows", 1, 5000)
            waitForRendering(page)

            const bar = treeScrollBar(page)
            tryCompare(bar, "size", 1, 5000, "rows that fit leave the bar no handle")
            tryVerify(() => {
                          const cell = view.itemAtCell(Qt.point(SurveyTreeModel.Name, 0))
                          return cell !== null
                              && Math.abs(cell.width - (view.width - view.fixedColumnsWidth)) <= 1
                      }, 5000, "the Name column takes all the room the fixed columns leave")
        }

        // The wide tree takes what the window has left below the action bar
        // and gives up height with the window, down to a floor at which the
        // page scrolls instead.
        function test_theWideNodeTreeFillsTheWindowDownToItsMinimum() {
            rootId.width = wideWindowWidth
            rootId.height = tallWindowHeight
            const nodes = setupCaveWithSection()
            const page = gotoNodePage("Source/Data/Node=Side Cave", nodes.cave)
            const view = tree(page, "caveDelegate0")
            tryCompare(view, "rows", 2, 5000)
            const treeHost = findChild(page, "tripTree")
            const flickable = pageFlickable(page)
            waitForRendering(page)

            const tallHeight = treeHost.height
            verify(flickable.contentHeight <= flickable.height,
                   "a tall window holds the whole page: content " + flickable.contentHeight
                   + " px in " + flickable.height + " px")
            const treeBottom = treeHost.mapToItem(flickable, 0, treeHost.height).y
            verify(Math.abs(flickable.height - Theme.pageMargin - treeBottom) <= 1,
                   "the tree reaches the window's bottom margin: its bottom sits at " + treeBottom
                   + " in a " + flickable.height + " px window")

            const shrink = tallWindowHeight - shortWindowHeight
            rootId.height = shortWindowHeight
            tryVerify(() => Math.abs(treeHost.height - (tallHeight - shrink)) <= 1, 5000,
                      "the tree shrinks with the window, from " + tallHeight + " to about "
                      + (tallHeight - shrink) + ", got " + treeHost.height)

            rootId.height = tinyWindowHeight
            tryCompare(treeHost, "height", Theme.treeMinimumHeight, 5000,
                       "a tiny window leaves the tree at its floor")
            tryVerify(() => flickable.contentHeight > flickable.height, 5000,
                      "below the floor the page scrolls: content " + flickable.contentHeight
                      + " px in " + flickable.height + " px")
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

        // The owner's repro: two Sections, each given a trip from its own
        // page, then the first Section's trip opened from the cave page's
        // tree. One node page item serves both Sections, so opening Trip 1
        // swaps that item back to the first Section on the way.
        function test_openingATripInTheFirstOfTwoSections() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            const cavePage = gotoNodePage("Source/Data/Node=Side Cave", cave)

            const firstSection = RootData.region.addNode(cave, SurveyNodeKind.Folder, "New Section")
            const firstSectionPage = gotoNodePage("Source/Data/Node=Side Cave/Node=New Section", firstSection)
            const firstTrip = firstSectionPage.addTripAndNavigate()
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.currentTrip === firstTrip,
                      5000, "Add Trip opens the first Section's trip")

            gotoNodePage("Source/Data/Node=Side Cave", cave)
            const secondSection = RootData.region.addNode(cave, SurveyNodeKind.Folder, "New Section")
            compare(secondSection.name, "New Section 2")
            const secondSectionPage = gotoNodePage("Source/Data/Node=Side Cave/Node=New Section 2", secondSection)
            const secondTrip = secondSectionPage.addTripAndNavigate()
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.currentTrip === secondTrip,
                      5000, "Add Trip opens the second Section's trip")

            gotoNodePage("Source/Data/Node=Side Cave", cave)
            const view = tree(cavePage, "caveDelegate0")
            tryCompare(view, "rows", 2, 5000)
            const firstSectionRow = findChild(cavePage, "caveDelegate0")
            compare(firstSectionRow.object, firstSection)
            firstSectionRow.toggleExpanded()
            tryCompare(view, "rows", 3, 5000)

            let tripRow = null
            tryVerify(() => {
                          tripRow = findChild(cavePage, "tripDelegate1")
                          return tripRow !== null && tripRow.object === firstTrip
                      }, 5000, "the first Section opens onto Trip 1")
            waitForRendering(tripRow)
            mouseClick(findChild(tripRow, "tripLink"))

            tryVerify(() => RootData.pageSelectionModel.currentPageAddress
                            === "Source/Data/Node=Side Cave/Node=New Section/Trip=" + firstTrip.name
                      && RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.currentTrip === firstTrip,
                      5000, "clicking Trip 1 opens its trip page")
            const tripPageItem = RootData.pageView.currentPageItem
            compare(tripPageItem.PageView.page, RootData.pageSelectionModel.currentPage,
                    "the trip page item holds the page the model shows")
        }

        // The node page's tree shares the Data page's expand/collapse button:
        // one click opens every Section, the next closes them again.
        function test_oneButtonTogglesTheNodeTreeRows() {
            const nodes = setupCaveWithSection()
            const page = gotoNodePage("Source/Data/Node=Side Cave", nodes.cave)

            const view = tree(page, "caveDelegate0")
            tryCompare(view, "rows", 2, 5000)

            const button = expandToggle(page)
            tryVerify(() => button.visible, 5000, "the button shows while a Section has trips")
            tryCompare(button, "text", "Expand all")

            mouseClick(button)
            tryCompare(view, "rows", 3, 5000, "expand all shows the Section's trip")
            tryCompare(button, "text", "Collapse all")

            mouseClick(button)
            tryCompare(view, "rows", 2, 5000, "collapse all closes the Section again")
            tryCompare(button, "text", "Expand all")
        }

        // A filter on the node page opens the Section that holds a match, and
        // clearing it puts the Section back the way it was.
        function test_filterOpensTheSectionHoldingAMatch() {
            const nodes = setupCaveWithSection()
            const page = gotoNodePage("Source/Data/Node=Side Cave", nodes.cave)

            const view = tree(page, "caveDelegate0")
            tryCompare(view, "rows", 2, 5000)

            const filter = findChild(findChild(page, "tripTree"), "surveyTreeFilter")
            verify(filter !== null, "the node page's tree has a filter field")

            filter.text = "survey"
            tryCompare(view, "rows", 3, 5000,
                       "both trips match, so the Section holding one opens")

            filter.text = ""
            tryCompare(view, "rows", 2, 5000, "clearing the filter closes the Section again")
        }

        // A node holding only trips has nothing to open, so the button hides.
        function test_expandToggleHidesOnATripOnlyNode() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            addTrip(cave, "Entrance survey", new Date(2023, 10, 12))
            const page = gotoNodePage("Source/Data/Node=Side Cave", cave)

            const view = tree(page, "tripDelegate0")
            tryCompare(view, "rows", 1, 5000)

            const button = expandToggle(page)
            tryVerify(() => !button.visible, 5000, "trip rows alone offer no expand/collapse")
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
                    ["addSectionMenuItem", "attachSurveyFileMenuItem", "addExternalTripMenuItem"],
                    "inside a cave the button adds the trip and the menu the rest")

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
            compare(visibleEntryNames(menu), ["addFolderMenuItem", "attachSurveyFileMenuItem"],
                    "a Folder outside every cave takes caves from the button and folders from the menu")
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
            verify(!findChild(page, "fixStationsBadge").visible,
                   "a node with no fix-station error shows no badge")

            mouseClick(link)
            tryVerify(() => {
                          const current = RootData.pageView.currentPageItem
                          return current !== null && current.objectName === "fixStationPage"
                      }, 5000, "the link opens the fix-station page")
            compare(RootData.pageView.currentPageItem.cave, folder, "the page edits the Folder's fixes")
        }

        // A cave's count spans its subtree: a Section's fixes are the cave's too.
        function test_aCavesFixStationLinkCountsItsSectionsFixes() {
            const caves = setupCaveWithSection()
            const page = gotoNodePage("Source/Data/Node=Side Cave", caves.cave)

            const link = findChild(page, "fixStationsLink")
            verify(link !== null && link.visible, "a cave page shows its fix stations")
            compare(link.text, "0")

            caves.section.fixStations.addFixStation()
            tryCompare(link, "text", "1", 5000, "the section's fix counts on the cave")

            caves.cave.fixStations.addFixStation()
            tryCompare(link, "text", "2", 5000, "the cave's own fix counts beside it")

            caves.section.fixStations.removeAt(0)
            tryCompare(link, "text", "1", 5000, "a removed section fix leaves the count")
        }

        // Gives \a node a shot from A to B and one fix on B whose easting has a
        // transposed leading digit: 1478000 in UTM 13N, outside the zone's
        // valid range.
        function addDomainBadFix(node) {
            addTripNamingStations(node, ["A", "B"])
            node.fixStations.addFixStation()
            const idx = node.fixStations.index(node.fixStations.count - 1)
            node.fixStations.setData(idx, "B", FixStationModel.StationNameRole)
            node.fixStations.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            node.fixStations.setData(idx, 1478000.0, FixStationModel.EastingRole)
            node.fixStations.setData(idx, 4430000.0, FixStationModel.NorthingRole)
            node.fixStations.setData(idx, 1655.0, FixStationModel.ElevationRole)
        }

        function outOfRangeMessagesNaming(badge, nodeName) {
            return badge.messages.filter(message => message.startsWith(nodeName + ": ")
                                         && message.indexOf("outside the valid range") >= 0)
        }

        // The badge covers what the count covers: a Section's bad fix is the
        // cave's warning too, named by the node it sits on.
        function test_aCavesFixStationBadgeShowsItsSectionsWarning() {
            const caves = setupCaveWithSection()
            addNode(null, SurveyNodeKind.Cave, "Other Cave")
            addDomainBadFix(caves.section)
            tryVerify(() => RootData.region.fixStationValidator.fixStationWarningMessages.length === 1,
                      5000, "the section's fix raises one project-wide warning")

            const page = gotoNodePage("Source/Data/Node=Side Cave", caves.cave)
            tryCompare(findChild(page, "fixStationsLink"), "text", "1", 5000)
            const badge = findChild(page, "fixStationsBadge")
            tryVerify(() => badge.visible, 5000,
                      "the cave's badge shows its section's warning; messages=" + JSON.stringify(badge.messages))
            compare(badge.messages.length, 1, "only the section's warning: " + badge.messages.join(" | "))
            compare(outOfRangeMessagesNaming(badge, "Upper level").length, 1,
                    "the line names the section: " + badge.messages.join(" | "))

            const errors = caves.section.errorModel.errors
            tryVerify(() => errors.count > 0, 5000)
            for (let row = 0; row < errors.count; ++row) {
                errors.setData(errors.index(row, 0), true, ErrorListModel.SuppressedRole)
            }
            tryVerify(() => !badge.visible, 5000,
                      "suppressing the section's warning hides the cave's badge")

            for (let row = 0; row < errors.count; ++row) {
                errors.setData(errors.index(row, 0), false, ErrorListModel.SuppressedRole)
            }
            tryVerify(() => badge.visible, 5000, "unsuppressing brings the cave's badge back")
        }

        function test_aSiblingCavesFixStationBadgeStaysHidden() {
            const caves = setupCaveWithSection()
            const other = addNode(null, SurveyNodeKind.Cave, "Other Cave")
            addDomainBadFix(caves.section)
            tryVerify(() => RootData.region.fixStationValidator.fixStationWarningMessages.length === 1,
                      5000, "the section's fix raises one project-wide warning")

            const sidePage = gotoNodePage("Source/Data/Node=Side Cave", caves.cave)
            const sideBadge = findChild(sidePage, "fixStationsBadge")
            verify(sideBadge !== null)
            tryVerify(() => sideBadge.visible, 5000, "the warning shows on its own cave's page")

            const page = gotoNodePage("Source/Data/Node=Other Cave", other)
            const badge = findChild(page, "fixStationsBadge")
            verify(badge !== null)
            verify(!badge.visible, "another cave's warning stays off this page: " + badge.messages.join(" | "))
            compare(badge.messages.length, 0)
        }

        // A warning reads the same on every page that shows it, the node's own
        // page included.
        function test_aSectionsOwnFixStationBadgeNamesTheSection() {
            const caves = setupCaveWithSection()
            addDomainBadFix(caves.section)

            const page = gotoNodePage("Source/Data/Node=Side Cave/Node=Upper level", caves.section)
            const badge = findChild(page, "fixStationsBadge")
            tryVerify(() => badge.visible, 5000, "the section's own warning shows its badge")
            tryCompare(badge.messages, "length", 1)
            compare(outOfRangeMessagesNaming(badge, "Upper level").length, 1,
                    "the own line carries the node-name prefix: " + badge.messages.join(" | "))
        }

        function test_movingTheSectionCarriesItsBadgeToTheNewCave() {
            const caves = setupCaveWithSection()
            const other = addNode(null, SurveyNodeKind.Cave, "Other Cave")
            addDomainBadFix(caves.section)
            tryVerify(() => RootData.region.fixStationValidator.fixStationWarningMessages.length === 1,
                      5000, "the section's fix raises one project-wide warning")

            const page = gotoNodePage("Source/Data/Node=Other Cave", other)
            const badge = findChild(page, "fixStationsBadge")
            verify(!badge.visible, "the section still sits under Side Cave")

            verify(other.moveHere(caves.section), "the section moves under Other Cave")
            tryVerify(() => badge.visible, 5000,
                      "the moved section's warning shows on its new cave; messages=" + JSON.stringify(badge.messages))
            compare(outOfRangeMessagesNaming(badge, "Upper level").length, 1,
                    "the line names the moved section: " + badge.messages.join(" | "))
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
            compare(findChild(kindMenu, "kindPickerFolder").text, "✓ Folder",
                    "where caves go the Folder entry reads Folder")
        }

        // Inside a cave a Folder is a Section, and the picker's entry says so.
        function test_thePickerCallsAFolderInsideACaveASection() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            const section = addNode(cave, SurveyNodeKind.Folder, "Upper Level")
            const page = gotoNodePage("Source/Data/Node=Side Cave/Node=Upper Level", section)

            const chip = findChild(page, "nodeKindChip")
            mouseClick(chip)
            const kindMenu = findChild(chip, "kindPickerMenu")
            tryVerify(() => kindMenu.opened, 5000, "the chip opens the picker")
            compare(findChild(kindMenu, "kindPickerFolder").text, "✓ Section")
            compare(findChild(kindMenu, "kindPickerCave").text, " Cave")
            kindMenu.close()
            tryVerify(() => !kindMenu.opened, 5000)
        }

        // The chip's own label agrees with its picker: a Folder inside a cave
        // reads Section, and Folder again once it moves to the top level.
        function test_theChipCallsAFolderInsideACaveASection() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            const section = addNode(cave, SurveyNodeKind.Folder, "Upper Level")
            const page = gotoNodePage("Source/Data/Node=Side Cave/Node=Upper Level", section)

            const chip = findChild(page, "nodeKindChip")
            tryCompare(chip, "text", "Section", 5000)

            RootData.region.moveNode(section, null, 0)
            tryVerify(() => section.parentNode !== cave, 5000, "the Section leaves the cave")
            tryCompare(chip, "text", "Folder", 5000)
        }

        // The header has no Rename button: the name edits inline on a
        // double-click. The harness never delivers doubleTapped, so the test
        // opens the editor the double-click opens.
        function test_noRenameButtonTheNameEditsInline() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            const page = gotoNodePage("Source/Data/Node=Side Cave", cave)

            compare(findChild(page, "renameNodeButton"), null, "the header carries no Rename button")
            const nameText = findChild(page, "nodeNameText")
            verify(!nameText.readOnly, "a native node's name is editable")
            nameText.openEditor()
            commitNewName("Fisher Ridge")
            tryCompare(cave, "name", "Fisher Ridge", 5000)
            tryCompare(nameText, "text", "Fisher Ridge", 5000)
        }

        // The Kind chip sits with the stats, above Trips, rather than in the
        // header beside the name.
        function test_kindChipSitsInTheStatsAboveTrips() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Side Cave")
            const page = gotoNodePage("Source/Data/Node=Side Cave", cave)

            const chip = findChild(page, "nodeKindChip")
            const statsBox = findChild(page, "nodeStatsBox")
            const tripCount = findChild(page, "tripCountValue")
            const name = findChild(page, "nodeNameText")
            tryVerify(() => chip.visible && statsBox.visible && tripCount.visible, 5000)

            const chipTop = chip.mapToItem(statsBox, 0, 0)
            verify(chipTop.x >= 0 && chipTop.y >= 0
                   && chipTop.x + chip.width <= statsBox.width
                   && chipTop.y + chip.height <= statsBox.height,
                   "the chip lies inside the stats box")
            verify(chip.mapToItem(null, 0, chip.height).y <= tripCount.mapToItem(null, 0, 0).y,
                   "the chip sits above Trips")
            verify(name.mapToItem(null, 0, name.height).y <= statsBox.mapToItem(null, 0, 0).y,
                   "the header, above the stats, holds the name alone")
        }

        // A Section's page header draws the signpost; a top-level Folder's the
        // folder; a Cave's the cave.
        function test_headerIconFollowsWhereTheNodeSits() {
            const nodes = setupCaveWithSection()
            let page = gotoNodePage("Source/Data/Node=Side Cave/Node=Upper level", nodes.section)
            let icon = findChild(page, "nodeKindIcon")
            tryVerify(() => String(icon.source).indexOf("signpost-split.svg") >= 0, 5000,
                      "a Section's header draws the signpost")
            verify(icon.visible)

            const folder = addNode(null, SurveyNodeKind.Folder, "Kentucky")
            page = gotoNodePage("Source/Data/Node=Kentucky", folder)
            icon = findChild(page, "nodeKindIcon")
            tryVerify(() => String(icon.source).indexOf("folder.svg") >= 0, 5000,
                      "a top-level Folder's header draws the folder")

            page = gotoNodePage("Source/Data/Node=Side Cave", nodes.cave)
            icon = findChild(page, "nodeKindIcon")
            tryVerify(() => String(icon.source).indexOf("caveKind.svg") >= 0, 5000,
                      "a Cave's header draws the cave")
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

        // Move to… on a row of the node page's tree opens the Data page with
        // the move armed, where the whole tree picks the destination; the top
        // level is offered as a button, since the region's root owns no row.
        function test_moveToFromThePageTreeArmsTheDataPage() {
            const nodes = setupCaveWithSection()
            const page = gotoNodePage("Source/Data/Node=Side Cave", nodes.cave)

            tree(page, "caveDelegate0")
            const sectionRow = findChild(page, "caveDelegate0")
            compare(sectionRow.object, nodes.section)
            sectionRow.showContextMenu(Theme.delegatePadding, sectionRow.height)
            let menu = null
            tryVerify(() => {
                          menu = findChild(sectionRow, "surveyItemContextMenu")
                          return menu !== null && menu.visible
                      }, 5000, "the Section row opens its context menu")
            const moveItem = findChild(menu, "surveyItemMoveMenuItem")
            verify(moveItem !== null && moveItem.visible, "a native node offers Move to…")
            mouseClick(moveItem)

            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "Move to… opens the Data page")
            verify(RegionSurveyTree.moveSubject === nodes.section, "the move carries the row's node")

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

        // A derived node's page names the file it comes from; the Kind chip names
        // its format and the breadcrumb carries the root. The root owns the fixed
        // positions, so the node offers none.
        function test_derivedNodeNamesTheFile() {
            const sourceRoot = attachCompassProject("node-page-derived")
            const alpha = childNamed(sourceRoot, "Alpha")
            verify(alpha !== null)
            const page = gotoNodePage("Source/Data/Node=PROJECT/Node=Alpha", alpha)

            const fileLabel = findChild(page, "derivedNodeFileLabel")
            tryVerify(() => fileLabel.visible, 5000, "a derived node names its source file")
            verify(!findChild(page, "sourceRootCard").visible, "the root's card stays on the root")
            compare(fileLabel.text, "PROJECT.MAK")
            compare(findChild(page, "derivedNodeFormatLabel"), null,
                    "XC.12a: the Kind chip names the format")

            verify(!findChild(page, "fixStationsLink").visible, "a derived node offers no fix stations")
            verify(!findChild(page, "fixStationsBadge").parent.visible,
                   "a derived node hides the whole fix-station group, badge included")
            const fixLabel = findChild(page, "derivedFixStationsLabel")
            verify(fixLabel.visible)
            compare(fixLabel.text, "Fixed positions come from the survey file")

            const crumbs = findChild(rootId.mainWindow, "linkBarListView")
            let rootCrumb = null
            tryVerify(() => {
                          for (let i = 0; i < crumbs.count; ++i) {
                              const item = crumbs.itemAtIndex(i)
                              if (item !== null && item.text === "PROJECT") {
                                  rootCrumb = findChild(item, "linkBarItemText")
                                  return rootCrumb !== null
                              }
                          }
                          return false
                      }, 5000, "the breadcrumb names the root")
            mouseClick(rootCrumb)
            tryVerify(() => RootData.pageSelectionModel.currentPageAddress === "Source/Data/Node=PROJECT"
                      && RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.currentNode === sourceRoot,
                      5000, "the root's crumb opens the root's page")
        }

        // What each drawn trip row's Decl cell shows, keyed by the trip's name:
        // "<caption> <angle>", or "—" for the em dash. Waits for `tripCount`.
        function declinationByTrip(page, view, tripCount) {
            let cells = {}
            tryVerify(() => {
                          cells = {}
                          let found = 0
                          for (let row = 0; row < view.rows; row++) {
                              const nameCell = findChild(page, "tripDelegate" + row)
                              const declCell = findChild(page, "tripDecl" + row)
                              if (nameCell === null || declCell === null) {
                                  continue
                              }
                              const name = findChild(nameCell, "tripLink").text.replace("📎 ", "")
                              cells[name] = declinationText(declCell)
                              found++
                          }
                          return found === tripCount
                      }, 5000, tripCount + " trip rows should draw their Decl cell")
            return cells
        }

        // survex_declination/declination.svx sets the declination one way per
        // block. Each derived trip row shows what cavern applies to it; the
        // node row, with no date, shows none.
        function test_attachedTripRowsShowTheFileDeclination() {
            const sourceRoot = makeSavedCaveAttach(
                                 "node-page-declination",
                                 "external-centerlines/survex_declination/declination.svx")
            const page = gotoNodePage("Source/Data/Node=AttachedCave", sourceRoot)
            const view = tree(page, "caveDelegate0")
            //parent, its trip child, and the root's five trips
            tryVerify(() => {
                          for (let row = 0; row < view.rows; row++) {
                              view.expand(row)
                          }
                          return view.rows === 7
                      }, 5000, "every node of the subtree is open; rows: " + view.rows)
            waitForRendering(page)

            const cells = declinationByTrip(page, view, 6)
            compare(cells["fileset"], "file -2.50°", "*calibrate declination 2.5 applies -2.50")
            compare(cells["modern"], "file 3.25°", "*declination 3.25 degrees applies +3.25")
            compare(cells["child"], "file -1.00°", "child inherits parent's *calibrate declination 1")
            verify(/^file auto -?\d+\.\d\d°$/.test(cells["autoset"]),
                   "*declination auto reads the caption, then IGRF to two decimals: "
                   + cells["autoset"])
            compare(cells["autoset"],
                    "file auto " + twoDecimals(tripNamed(sourceRoot, "autoset").appliedDeclination) + "°",
                    "the number is the declination the trip applies")
            compare(cells["silent"], "none 0.00°", "a block that sets nothing applies 0")
            const emDashTrips = Object.keys(cells).filter(name => cells[name] === "—")
            compare(emDashTrips, ["mixed"],
                    "only the block that changes declination mid-block has no one value")

            for (let row = 0; row < view.rows; row++) {
                const nodeDecl = findChild(page, "caveDecl" + row)
                verify(nodeDecl === null || findChild(nodeDecl, "declinationMode") === null,
                       "a node row draws no declination")
            }
        }

        // A sourced node takes native trips and sections and further files,
        // never a trip copied from a file of its own.
        function test_addVerbsOnASourcedNode() {
            const sourceRoot = attachCompassProject("node-page-sourced-add")
            const alpha = childNamed(sourceRoot, "Alpha")

            for (const entry of [{ address: "Source/Data/Node=PROJECT", node: sourceRoot },
                                 { address: "Source/Data/Node=PROJECT/Node=Alpha", node: alpha }]) {
                const page = gotoNodePage(entry.address, entry.node)
                compare(findChild(findChild(page, "addTrip"), "addButton").text, "Add Trip",
                        entry.address + " adds a trip from the button")
                const menu = openAddCaret(page)
                compare(visibleEntryNames(menu), ["addSectionMenuItem", "attachSurveyFileMenuItem"],
                        entry.address + " offers Add Section and Attach survey file… beside it")
                menu.close()
                tryVerify(() => !menu.opened, 5000)
            }
        }
    }
}
