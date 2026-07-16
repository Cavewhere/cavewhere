import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "SourcePage"
        when: windowShown

        function init() {
            RootData.recentProjectModel.clear();
        }

        function test_newProjectMenu_navigatesToView() {
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow);

            let addButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->addRepositoryButton")
            mouseClick(addButton)
            tryVerify(() => { return findChild(mainWindow, "addMenuNew") !== null })
            let addMenuNew = findChild(mainWindow, "addMenuNew")
            mouseClick(addMenuNew)

            tryVerify(() => {
                return RootData.pageSelectionModel.currentPageAddress === "View"
            }, 5000, "should navigate to View after creating a new project")
        }

        function test_openV6CavewhereFile() {
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow);

            //Copy the test project to the test folder
            let filename = TestHelper.copyToTempDir(TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            // let openCavingAreaButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->openCavingAreaButton")
            // mouseClick(openCavingAreaButton_obj1, 61.707, 18.5938)

            console.log("Temp file:" + filename)

            //Simulating opening a file
            let dialog = ObjectFinder.findObjectByChain(mainWindow, "rootId->loadProjectDialog")
            dialog.runLoadForTest(TestHelper.toLocalUrl(filename))

            //Make we have all the data
            //Legacy SQLite .cw files convert in place to a .cw bundle (issue #515),
            //so fileType is BundledGitFileType and save() re-zips over the source path.
            tryVerify(() => { return RootData.region.caveCount > 0 }, 20000)
            tryCompare(RootData.project, "fileType", Project.BundledGitFileType, 5000)

            verify(RootData.project.save())
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            TestHelper.waitForFutureManagerToFinish(RootData.futureManagerModel)

            compare(RootData.project.fileType, Project.BundledGitFileType)

            //Save as a .cw bundle and verify it becomes BundledGitFileType
            let bundlePath = RootData.urlToLocal(TestHelper.tempDirectoryUrl()) + "/Phake Cave 3000.cw"
            verify(RootData.project.saveAs(bundlePath))
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            TestHelper.waitForFutureManagerToFinish(RootData.futureManagerModel)

            compare(RootData.project.fileType, Project.BundledGitFileType)
        }

        function test_openLegacyProject_showsOnlyOriginalBundlePath() {
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow);

            let filename = TestHelper.copyToTempDir(TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            let dialog = ObjectFinder.findObjectByChain(mainWindow, "rootId->loadProjectDialog")
            dialog.runLoadForTest(TestHelper.toLocalUrl(filename))

            tryVerify(() => { return RootData.region.caveCount > 0 }, 20000)

            // loadProject() navigates to View page; go back to Source to access the list
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow);

            let repoListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->repositoryListView")
            tryVerify(() => { return repoListView.count === 1 })
            waitForRendering(repoListView)

            let firstDelegate = repoListView.itemAtIndex(0)
            compare(firstDelegate.pathRole, filename)
        }

        function test_openRecentItem_asksToSave_whenProjectModified() {
            // Save a project so it appears in the recent list.
            const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            const projectPath = tmpPath + "/source-ask-save.cwproj"
            verify(RootData.project.saveAs(projectPath))
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            // Make the project modified.
            RootData.region.addCave()
            tryVerify(() => RootData.project.modified, 3000,
                      "project should become modified after adding a cave")

            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow)

            let repoListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->repositoryListView")
            tryVerify(() => repoListView.count > 0, 3000, "recent list should have an entry")
            waitForRendering(repoListView)

            // Click the recent item's name link.
            let linkText = findChild(mainWindow, "repoLinkText_0")
            verify(linkText !== null, "repoLinkText_0 not found")
            mouseClick(linkText)

            // AskToSaveDialog should appear.
            let askToSaveDialog = findChild(rootId, "mainWindowTestAskToSaveDialog")
            verify(askToSaveDialog !== null, "mainWindowTestAskToSaveDialog not found")
            tryVerify(() => {
                return askToSaveDialog._dialog !== null
                       && askToSaveDialog._dialog.askToSaveDialog.visible
            }, 5000, "AskToSaveDialog should appear when opening a recent item with unsaved changes")

            // Discard and verify the project loads.
            askToSaveDialog._dialog.askToSaveDialog.discarded()

            tryVerify(() => {
                return RootData.pageSelectionModel.currentPageAddress === "View"
            }, 10000, "should navigate to View after discarding")
        }

        function test_openSavedBundle_showsBundlePathInsteadOfTempDirectory() {
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow);

            let filename = TestHelper.copyToTempDir(TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            let dialog = ObjectFinder.findObjectByChain(mainWindow, "rootId->loadProjectDialog")
            dialog.runLoadForTest(TestHelper.toLocalUrl(filename))

            tryVerify(() => { return RootData.region.caveCount > 0 }, 20000)

            RootData.recentProjectModel.clear()
            compare(RootData.recentProjectModel.rowCount(), 0)

            let tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            let bundledPath = tempDir + "/source-page-saveas-bundle.cw"
            verify(RootData.project.saveAs(bundledPath))
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            RootData.recentProjectModel.clear()
            compare(RootData.recentProjectModel.rowCount(), 0)

            dialog.runLoadForTest(TestHelper.toLocalUrl(bundledPath))
            tryVerify(() => { return RootData.region.caveCount > 0 }, 20000)

            let repoListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->repositoryListView")
            tryVerify(() => { return repoListView.count === 1 })
            waitForRendering(repoListView)

            let firstDelegate = repoListView.itemAtIndex(0)
            compare(firstDelegate.pathRole, bundledPath)

            mouseClick(firstDelegate)
            tryVerify(() => { return repoListView.count === 1 })
            waitForRendering(repoListView)

            firstDelegate = repoListView.itemAtIndex(0)
            compare(firstDelegate.pathRole, bundledPath)
        }
    }
}
