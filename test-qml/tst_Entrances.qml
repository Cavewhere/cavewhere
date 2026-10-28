import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Entrance markers in the 3D view: every placed, solved fix gets an
// EntrancePoint from the renderer's EntranceView, and its Type keyword hides it.
// There is no global toggle; keywords are the only switch.
MainWindowTest {
    id: rootId

    LinkGenerator {
        id: linkGeneratorId
    }

    TestCase {
        id: entrancesTestCase
        name: "Entrances"
        when: windowShown

        // A fresh project solves through cavern and the line plot before a fix
        // has a position; several test processes share the machine at night.
        readonly property int solveTimeout: 20000

        function init() {
            rootId.closeAnyOpenEditor()

            RootData.futureManagerModel.waitForFinished()
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "viewPage"
            }, 5000, "should be on view page at start of test")
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        function entranceView() {
            return ObjectFinder.findObjectByChain(
                        mainWindow, "rootId->viewPage->SplitView->renderer->entranceView")
        }

        // One cave with one shot from A1 to A2 and a fix on A1 in Kentucky.
        function addCaveWithEntrance(name) {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.rowCount() - 1)
            cave.name = name
            cave.addTrip()
            const trip = cave.trip(0)
            trip.addNewChunk()
            const chunk = trip.chunk(0)
            chunk.setData(SurveyChunk.StationNameRole, 0, "A1")
            chunk.setData(SurveyChunk.StationNameRole, 1, "A2")
            chunk.setData(SurveyChunk.ShotDistanceRole, 0, "10")
            chunk.setData(SurveyChunk.ShotCompassRole, 0, "0")
            chunk.setData(SurveyChunk.ShotClinoRole, 0, "-5")

            const fixes = cave.fixStations
            const row = fixes.addFixStation("A1")
            compare(row, 0)
            fixes.setData(fixes.index(row), "EPSG:4326", FixStationModel.InputCSRole)
            compare(fixes.setCoordinateText(row, "37.0, -84.0, 300m", Units.Metric), "")
            return cave
        }

        // Adds one solved cave with a fix on A1 and waits for its visible
        // EntrancePoint. Returns {cave, point}.
        function addSolvedEntrance(name) {
            const view = entranceView()
            verify(view !== null, "the renderer should host an EntranceView")

            const cave = addCaveWithEntrance(name)
            tryVerify(() => cave.length.value > 0, entrancesTestCase.solveTimeout,
                      "the solve should give the cave a length")

            let point = null
            tryVerify(() => {
                point = findChild(view, "entrancePoint_A1")
                return point !== null && point.visible
            }, entrancesTestCase.solveTimeout, "the solved fix should get a visible entrance point")
            return {cave: cave, point: point}
        }

        // The marker has to be on screen for a click to reach it.
        function verifyMarkerOnScreen(point) {
            const renderer = ObjectFinder.findObjectByChain(
                               mainWindow, "rootId->viewPage->SplitView->renderer")
            tryVerify(() => {
                const center = point.marker.mapToItem(renderer, point.marker.width / 2,
                                                      point.marker.height / 2)
                return renderer.contains(center)
            }, 5000, "the entrance marker should be inside the renderer")
        }

        // Taps the marker until the entrance selects, then waits for the async
        // popup Loader. A single tap can miss while the offscreen view settles, and
        // a tap after selecting would toggle it closed, so the retry checks first.
        // Returns the quote box.
        function openEntrancePopup(point) {
            verifyMarkerOnScreen(point)
            tryVerify(() => {
                if (point.selected) {
                    return true
                }
                mouseClick(point.marker)
                return point.selected
            }, 5000, "the entrance should select after tapping its marker")

            let quoteBox = null
            tryVerify(() => {
                quoteBox = findChild(point, "entranceQuoteBox")
                return quoteBox !== null
            }, 5000, "the entrance popup should open after selecting the marker")
            return quoteBox
        }

        // Opens the Layers tab, sets the first filter row to the Type key and
        // returns the keyword-list checkbox for the Type value (the same walk
        // tst_Leads.qml's typeValueCheckbox makes).
        function typeValueCheckbox(value) {
            let layersTab = findChild(rootId.mainWindow, "layersTabButton")
            verify(layersTab !== null)
            mouseClick(layersTab)

            let delegate = null
            tryVerify(() => {
                delegate = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0")
                return delegate !== null
            })

            let keyCombo = findChild(delegate, "keyCombo")
            verify(keyCombo)
            keyCombo.currentIndex = keyCombo.model.indexOf("Type")
            delegate.filterModelObjectRole.key = keyCombo.currentText
            verify(delegate.filterModelObjectRole.key === "Type")

            let filterModel = delegate.filterModelObjectRole
            let rowIndex = -1
            tryVerify(() => {
                for(let i = 0; i < filterModel.rowCount(); ++i) {
                    let rowValue = filterModel.data(filterModel.index(i, 0), KeywordGroupByKeyModel.ValueRole)
                    if(rowValue === value) {
                        rowIndex = i
                        return true
                    }
                }
                return false
            }, 5000, "there should be a '" + value + "' Type row")

            let checkbox = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->keywordList->row" + rowIndex + "->checkbox")
            verify(checkbox)

            // The group list is sized on a deferred polish pass; a click before it
            // lands outside the clipping list and reaches nothing.
            let groupList = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->keyword->groupListView")
            tryVerify(() => {
                let center = checkbox.mapToItem(groupList, checkbox.width / 2, checkbox.height / 2)
                return groupList.contains(center)
            }, 5000, "keyword list should be laid out before the checkbox is clicked")
            return checkbox
        }

        // The renderer styles the name line a step above station labels and the
        // stats line under it smaller and gray, with the outline that flips with
        // the theme, all through Theme tokens.
        function test_entranceTextStylesComeFromTheme() {
            const view = entranceView()
            verify(view !== null, "the renderer should host an EntranceView")

            verify(Theme.entranceNameFontSize > Theme.fontSizeUI,
                   "an entrance name should be larger than a station label")
            verify(Theme.entranceStatsFontSize < Theme.entranceNameFontSize,
                   "the stats line should be smaller than the name")
            verify(Qt.colorEqual(Theme.entranceStatsColor, Theme.textSecondary),
                   "the stats line should be the secondary text gray")
            verify(Qt.colorEqual(Theme.entranceStatsOutlineColor, Theme.textInverse),
                   "the stats outline should be the inverse text color")

            compare(view.font.pixelSize, Theme.entranceNameFontSize)
            verify(Qt.colorEqual(view.color, Theme.textInverse), "the name keeps the inverse text fill")
            verify(Qt.colorEqual(view.outlineColor, Theme.text), "the name keeps the text outline")

            compare(view.statsFont.pixelSize, Theme.entranceStatsFontSize)
            verify(Qt.colorEqual(view.statsColor, Theme.entranceStatsColor),
                   "the view's stats fill should come from Theme.entranceStatsColor")
            verify(Qt.colorEqual(view.statsOutlineColor, Theme.entranceStatsOutlineColor),
                   "the view's stats outline should come from Theme.entranceStatsOutlineColor")
        }

        // The renderer's entrances and station labels share the scene's declutter
        // pass, entrances first.
        function test_entrancesAndStationLabelsJoinTheDeclutterPass() {
            const view = entranceView()
            verify(view !== null, "the renderer should host an EntranceView")
            compare(view.declutterTier, LabelDeclutter.EntranceTier)

            const siblings = view.parent.children
            let labelViews = []
            for (let i = 0; i < siblings.length; ++i) {
                if (siblings[i] instanceof LinePlotLabelView) {
                    labelViews.push(siblings[i])
                }
            }
            compare(labelViews.length, 1, "the renderer should host one LinePlotLabelView")
            compare(labelViews[0].declutterTier, LabelDeclutter.StationLabelTier)
        }

        // Unchecking the Entrance Type hides the fix's marker and rechecking it
        // brings the marker back. Hiding keeps the entrance in the view's drawn
        // set; only its pin, text and tap target go.
        function test_entranceTypeKeywordHidesTheMarker() {
            const view = entranceView()
            const {point} = addSolvedEntrance("Gypsum Hollow")
            compare(view.entranceCount, 1)

            const checkbox = typeValueCheckbox("Entrance")

            mouseClick(checkbox)
            tryVerify(() => !point.visible, 5000,
                      "the entrance should hide when the Entrance Type is filtered out")
            compare(view.entranceCount, 1)

            mouseClick(checkbox)
            tryVerify(() => point.visible, 5000,
                      "the entrance should return when the Entrance Type is re-enabled")
        }

        // Tapping the pin opens a popup naming the entrance, its kind, and the
        // cave's length and depth.
        function test_tapEntranceShowsPopup() {
            const {point} = addSolvedEntrance("Gypsum Hollow")

            const quoteBox = openEntrancePopup(point)
            verify(quoteBox.pointAtObject === point.marker, "the popup should point at the pin")
            verify(entranceView().selectionManager.selectedItem === point,
                   "the tapped entrance should be the view's selection")

            tryCompare(findChild(quoteBox, "entranceName"), "text", "Gypsum Hollow")
            tryCompare(findChild(quoteBox, "entranceKind"), "text", "Entrance")

            for (const statName of ["entranceLength", "entranceDepth"]) {
                const stat = findChild(quoteBox, statName)
                verify(stat !== null, statName + " should be in the popup")
                verify(stat.visible, statName + " should show for an Entrance")
                const value = findChild(stat, "value")
                verify(parseFloat(value.text) > 0, statName + " should show the cave's value")
            }
        }

        // A Well's popup names it and its kind, without the cave's length and depth.
        function test_wellPopupShowsNameAndKind() {
            const {cave, point} = addSolvedEntrance("Gypsum Hollow")
            const fixes = cave.fixStations
            verify(fixes.setData(fixes.index(0), FixStation.Well, FixStationModel.KindRole))
            verify(fixes.setData(fixes.index(0), "North Sink", FixStationModel.LabelRole))
            tryVerify(() => point.kind === FixStation.Well && point.displayName === "North Sink",
                      5000, "the point should follow the fix's kind and label")

            const quoteBox = openEntrancePopup(point)
            tryCompare(findChild(quoteBox, "entranceName"), "text", "North Sink")
            tryCompare(findChild(quoteBox, "entranceKind"), "text", "Well")
            for (const statName of ["entranceLength", "entranceDepth"]) {
                const stat = findChild(quoteBox, statName)
                verify(stat === null || !stat.visible, statName + " should be hidden for a Well")
            }
        }

        // The popup's link opens the owning cave's page.
        function test_entranceLinkOpensNodePage() {
            const {cave, point} = addSolvedEntrance("Gypsum Hollow")
            const expectedAddress = linkGeneratorId.nodeLink(cave)
            verify(expectedAddress !== "")

            const quoteBox = openEntrancePopup(point)
            const link = findChild(quoteBox, "entranceNodeLink")
            verify(link !== null, "the popup should have a link to the cave")
            verify(link.text.indexOf("Gypsum Hollow") >= 0, "the link should name the cave")

            mouseClick(link)
            tryCompare(RootData.pageSelectionModel, "currentPageAddress", expectedAddress, 5000)
        }

        // Tapping empty space closes the popup, as it does for leads.
        function test_tapAwayClosesEntrancePopup() {
            const {point} = addSolvedEntrance("Gypsum Hollow")
            openEntrancePopup(point)

            const renderer = ObjectFinder.findObjectByChain(
                               mainWindow, "rootId->viewPage->SplitView->renderer")
            const pinPosition = point.marker.mapToItem(renderer, 0, 0)
            const cornerX = (pinPosition.x > renderer.width * 0.5) ? 5 : renderer.width - 5
            const cornerY = (pinPosition.y > renderer.height * 0.5) ? 5 : renderer.height - 5
            mouseClick(renderer, cornerX, cornerY)

            tryVerify(() => !point.selected, 2000, "the entrance should deselect after tapping away")
            tryVerify(() => findChild(point, "entranceQuoteBox") === null, 2000,
                      "the popup should close after tapping away")
        }

        // Tapping the selected pin again closes its popup.
        function test_tapSelectedEntranceTogglesClosed() {
            const {point} = addSolvedEntrance("Gypsum Hollow")
            openEntrancePopup(point)

            mouseClick(point.marker)
            tryVerify(() => !point.selected, 5000, "tapping the selected entrance should close it")
            tryVerify(() => findChild(point, "entranceQuoteBox") === null, 5000,
                      "the popup should close on the second tap")
        }

        // Filtering out the Entrance Type closes the open popup, and the hidden
        // pin ignores taps until the filter brings it back.
        function test_hiddenEntranceIsUntappable() {
            const {point} = addSolvedEntrance("Gypsum Hollow")
            openEntrancePopup(point)
            const selectionManager = entranceView().selectionManager

            const checkbox = typeValueCheckbox("Entrance")
            mouseClick(checkbox)
            tryVerify(() => !point.visible, 5000,
                      "the entrance should hide when the Entrance Type is filtered out")
            tryVerify(() => selectionManager.selectedItem === null, 5000,
                      "hiding the selected entrance should close its popup")

            // The tap must land on nothing, so a fixed wait is the only way to see
            // that the selection stays clear.
            mouseClick(point.marker)
            wait(250)
            verify(!point.selected, "a hidden entrance should ignore the tap")
            verify(selectionManager.selectedItem === null,
                   "nothing should be selected after tapping a hidden entrance")
            verify(findChild(point, "entranceQuoteBox") === null,
                   "no popup should open for a hidden entrance")
        }
    }
}
