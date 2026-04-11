import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "DiscardReload"
        when: windowShown

        function cleanup() {
            RootData.pageSelectionModel.gotoPageByName(null, "View")
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "viewPage"
            }, 5000, "return to view page during cleanup")
            RootData.newProject()
        }

        // Reproduces a bug where discardChangesAndReload() navigates to the
        // View page but stale QML state persists: the scrap remains selected
        // with its moved station position, modifications no longer mark the
        // project dirty, and editing survey data crashes with
        // "Unregistering a null page!" / QList::at index out of range.
        function test_scrapStationRestoredAfterDiscardReload() {
            // -- 1. Load Phake Cave 3000 and save as .cwproj (git-backed) --
            RootData.account.name = "Discard Test"
            RootData.account.email = "discard.test@example.com"

            TestHelper.loadProjectFromFile(RootData.project,
                "://datasets/test_cwProject/Phake Cave 3000.cw")
            RootData.futureManagerModel.waitForFinished()
            tryVerify(() => { return RootData.region.caveCount > 0 }, 10000)

            const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            const projectPath = tmpPath + "/discard-reload-test.cwproj"
            verify(RootData.project.saveAs(projectPath), "saveAs should succeed")
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            // save() on a git project flushes + commits, giving us a HEAD to discard to
            verify(RootData.project.save(), "initial save/commit should succeed")
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            verify(!RootData.project.modified, "project should not be modified after commit")

            // -- 2. Navigate to first trip and enter carpet (scrap) mode --
            let cave = RootData.region.cave(0)
            let trip = cave.trip(0)
            let caveName = String(cave.name)
            let tripName = String(trip.name)
            let tripAddress = "Source/Data/Cave=" + caveName + "/Trip=" + tripName

            RootData.pageSelectionModel.currentPageAddress = tripAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
            }, 10000, "should navigate to trip page")

            let carpetButton = ObjectFinder.findObjectByChain(
                mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(carpetButton)
            wait(300)

            // -- 3. Select the first scrap and record original station position --
            let noteArea = ObjectFinder.findObjectByChain(
                mainWindow, "rootId->tripPage->noteGallery->noteArea")
            let scrapView = findChild(noteArea, "scrapViewId")
            verify(scrapView !== null, "scrapView must exist")
            scrapView.selectScrapIndex = 0
            tryVerify(() => scrapView.selectedScrapItem !== null, 3000,
                      "first scrap should be selected")

            let scrap = scrapView.selectedScrapItem.scrap
            verify(scrap !== null, "scrap data object must exist")
            verify(scrap.numberOfStations() > 0, "scrap must have at least one station")

            let originalPos = scrap.stationData(Scrap.StationPosition, 0)
            let originalName = String(scrap.stationData(Scrap.StationName, 0))

            // -- 4. Move the station to a different position --
            let movedPos = Qt.point(originalPos.x + 0.05, originalPos.y + 0.05)
            scrap.setStationData(Scrap.StationPosition, 0, movedPos)

            let afterMovePos = scrap.stationData(Scrap.StationPosition, 0)
            fuzzyCompare(afterMovePos.x, movedPos.x, 0.001)
            fuzzyCompare(afterMovePos.y, movedPos.y, 0.001)

            // Wait for the autosave to flush so git sees the change
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            tryVerify(() => RootData.project.modified, 5000,
                      "project should be modified after moving station")

            // -- 5. Discard changes and reload the project --
            let discardDone = false
            RootData.project.discardCompleted.connect(function() {
                discardDone = true
            })

            RootData.project.discardChanges()
            tryVerify(() => discardDone, 10000, "discard should complete")

            RootData.loadProject(RootData.project.filename)

            tryVerify(() => {
                return RootData.pageSelectionModel.currentPageAddress === "View"
            }, 10000, "should navigate to View after discard+reload")

            tryVerify(() => { return RootData.region.caveCount > 0 }, 10000,
                      "region should have caves after reload")

            RootData.futureManagerModel.waitForFinished()

            // -- 6. Navigate back to the same trip --
            cave = RootData.region.cave(0)
            trip = cave.trip(0)
            verify(cave !== null, "cave must exist after reload")
            verify(trip !== null, "trip must exist after reload")

            // Navigate step by step so QML components register their subpages
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "dataMainPage"
            }, 10000, "should navigate to data page")

            let caveAddress = "Source/Data/Cave=" + String(cave.name)
            RootData.pageSelectionModel.currentPageAddress = caveAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "cavePage"
            }, 10000, "should navigate to cave page")

            tripAddress = "Source/Data/Cave=" + String(cave.name) + "/Trip=" + String(trip.name)
            RootData.pageSelectionModel.currentPageAddress = tripAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
            }, 10000, "should navigate to trip page after reload")

            // Enter carpet mode again
            carpetButton = ObjectFinder.findObjectByChain(
                mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(carpetButton)
            wait(300)

            // -- 7. Verify the station is back to its original position --
            // Re-query noteArea and scrapView inside tryVerify so we
            // pick up the fresh objects once carpet mode finishes.
            tryVerify(() => {
                noteArea = ObjectFinder.findObjectByChain(
                    mainWindow, "rootId->tripPage->noteGallery->noteArea")
                scrapView = findChild(noteArea, "scrapViewId")
                return scrapView !== null && scrapView.count > 0
            }, 5000, "scrapView should have scraps after reload")
            scrapView.selectScrapIndex = 0
            tryVerify(() => {
                          return scrapView.selectedScrapItem !== null
                                 && scrapView.selectedScrapItem.scrap !== null
                      }, 5000, "first scrap should be selected after reload")

            let reloadedScrap = scrapView.selectedScrapItem.scrap
            verify(reloadedScrap !== null, "scrap data object must exist after reload")
            verify(reloadedScrap.numberOfStations() > 0,
                   "scrap must have stations after reload")

            let restoredPos = reloadedScrap.stationData(Scrap.StationPosition, 0)
            let restoredName = String(reloadedScrap.stationData(Scrap.StationName, 0))

            compare(restoredName, originalName,
                    "station name should be restored to committed state")
            fuzzyCompare(restoredPos.x, originalPos.x, 0.001,
                         "station X should be restored (was " + restoredPos.x
                         + ", expected " + originalPos.x + ")")
            fuzzyCompare(restoredPos.y, originalPos.y, 0.001,
                         "station Y should be restored (was " + restoredPos.y
                         + ", expected " + originalPos.y + ")")

            // -- 8. Verify the project modification tracking still works --
            verify(!RootData.project.modified,
                   "project should not be modified after reload")

            reloadedScrap.setStationData(Scrap.StationPosition, 0, movedPos)
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            tryVerify(() => RootData.project.modified, 5000,
                      "project should become modified after editing reloaded scrap")
        }
    }
}
