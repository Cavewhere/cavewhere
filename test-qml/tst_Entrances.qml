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

        // Unchecking the Entrance Type hides the fix's marker and rechecking it
        // brings the marker back. Hiding keeps the entrance in the view's drawn
        // set; only its pin, text and tap target go.
        function test_entranceTypeKeywordHidesTheMarker() {
            const view = entranceView()
            verify(view !== null, "the renderer should host an EntranceView")

            const cave = addCaveWithEntrance("Gypsum Hollow")
            tryVerify(() => cave.length.value > 0, entrancesTestCase.solveTimeout,
                      "the solve should give the cave a length")

            let point = null
            tryVerify(() => {
                point = findChild(view, "entrancePoint_A1")
                return point !== null && point.visible
            }, entrancesTestCase.solveTimeout, "the solved fix should get a visible entrance point")
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
    }
}
