import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "ProjectPageReload"
        when: windowShown

        // Reproduces GitHub issue #369: after loading a project, navigating
        // to a trip page, then opening a recent project from SourceListPage,
        // clicking "Data" in the sidebar should show DataMainPage — not the
        // old trip page from the previous project.
        //
        // The bug is that SourceListPage's recent-project onClicked handler
        // calls loadFile + gotoPageByName(View) but does NOT clear history.
        // So the sidebar's findPage() still finds the old trip address in
        // history and navigates there instead of DataMainPage.
        function test_oldTripPageNotShownAfterOpeningRecentProject() {
            // 1. Load Project 1 (Phake Cave 3000) and save it so it
            //    appears in the recent projects list.
            RootData.recentProjectModel.clear()
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");
            RootData.futureManagerModel.waitForFinished();
            tryVerify(() => { return RootData.region.caveCount > 0 }, 20000)

            const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            const projectPath = tmpPath + "/issue369-test.cwproj"
            verify(RootData.project.saveAs(projectPath))
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            // 2. Navigate to a trip page
            let cave = RootData.region.cave(0)
            let trip = cave.trip(0)
            let tripAddress = "Source/Data/Cave=" + cave.name + "/Trip=" + trip.name
            RootData.pageSelectionModel.currentPageAddress = tripAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
            }, 10000, "Should navigate to the trip page")
            waitForRendering(mainWindow)

            // Make the project modified so AskToSaveDialog appears
            RootData.region.addCave()
            tryVerify(() => RootData.project.modified, 3000,
                      "project should become modified after adding a cave")

            // 3. Go to Source page and click the recent project link
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow)

            let repoListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->repositoryListView")
            tryVerify(() => repoListView.count > 0, 3000, "recent list should have an entry")
            waitForRendering(repoListView)

            let linkText = findChild(mainWindow, "repoLinkText_0")
            verify(linkText !== null, "repoLinkText_0 not found")
            mouseClick(linkText)

            // 4. Handle the AskToSaveDialog — discard changes
            let askToSaveDialog = findChild(rootId, "mainWindowTestAskToSaveDialog")
            verify(askToSaveDialog !== null, "mainWindowTestAskToSaveDialog not found")
            tryVerify(() => {
                return askToSaveDialog._dialog !== null
                       && askToSaveDialog._dialog.askToSaveDialog.visible
            }, 5000, "AskToSaveDialog should appear")

            askToSaveDialog._dialog.askToSaveDialog.discarded()

            // Should navigate to View after loading the project
            tryVerify(() => {
                return RootData.pageSelectionModel.currentPageAddress === "View"
            }, 10000, "Should navigate to View after opening recent project")

            // 5. Click Data in the sidebar — the exact user action from the bug report
            let dataButton = findChild(mainWindow, "dataButton")
            verify(dataButton !== null, "Data sidebar button must exist")
            mouseClick(dataButton)

            // 6. Should see DataMainPage, NOT the old trip page
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "dataMainPage"
            }, 5000, "Clicking Data should show DataMainPage, not old trip page from previous project. "
                     + "Current address: " + RootData.pageSelectionModel.currentPageAddress)
        }
    }
}
