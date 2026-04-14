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

        function test_addRepository() {
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow);

            let addButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->addRepositoryButton")
            mouseClick(addButton)
            tryVerify(() => { return findChild(mainWindow, "addMenuNew") !== null })
            let addMenuNew = findChild(mainWindow, "addMenuNew")
            mouseClick(addMenuNew)

            let tempDir = TestHelper.tempDirectoryUrl();
            RootData.recentProjectModel.defaultRepositoryDir = tempDir

            let dirText = findChild(mainWindow, "repositoryParentDir")
            compare(dirText.text, RootData.urlToLocal(RootData.recentProjectModel.defaultRepositoryDir))
            compare(dirText.text, RootData.urlToLocal(tempDir))

            //Write test
            let nameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "Popup->cavingAreaName->TextField")
            mouseClick(nameTextEdit)

            //Write test
            keyClick("s")
            keyClick("a")
            keyClick("u")
            keyClick("c")
            keyClick("e")

            //Add the test repository
            keyClick(16777217, 0) //Tab
            keyClick(32, 0) //Space

            //Make sure we have the repository and it's created
            let repoListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->repositoryListView")
            compare(repoListView.count, 1)
            waitForRendering(repoListView);

            let firstDelegate = repoListView.itemAtIndex(0);
            compare(firstDelegate.nameRole, "sauce")

        }

        function test_emptyNameError() {
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow);

            let addButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->addRepositoryButton")
            mouseClick(addButton)
            tryVerify(() => { return findChild(mainWindow, "addMenuNew") !== null })
            let addMenuNew = findChild(mainWindow, "addMenuNew")
            mouseClick(addMenuNew)

            let tempDir = TestHelper.tempDirectoryUrl();
            RootData.recentProjectModel.defaultRepositoryDir = tempDir

            let dirText = ObjectFinder.findObjectByChain(mainWindow, "Popup->repositoryParentDir")
            compare(dirText.text, RootData.urlToLocal(RootData.recentProjectModel.defaultRepositoryDir))
            compare(dirText.text, RootData.urlToLocal(tempDir))

            //Make sure the error isn't visible yet
            let cavingAreaNameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "Popup->cavingAreaName")
            compare(cavingAreaNameTextEdit.textField.text, "")
            compare(cavingAreaNameTextEdit.hasError, false)


            //User clicks okay
            let label_obj2 = ObjectFinder.findObjectByChain(mainWindow, "Popup->openRepoButton")
            mouseClick(label_obj2)

            //Error should be shown
            compare(cavingAreaNameTextEdit.hasError, true)
            compare(cavingAreaNameTextEdit.errorMessage, "Caving area name is empty")

            //Write test
            let nameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "Popup->cavingAreaName->TextField")
            mouseClick(nameTextEdit)

            //Write test
            keyClick("t")
            //Make sure the error goes away
            compare(cavingAreaNameTextEdit.errorMessage, "")
            compare(cavingAreaNameTextEdit.hasError, false)

            keyClick("e")
            keyClick("s")
            keyClick("t")

            //Add the test repository
            keyClick(16777217, 0) //Tab
            keyClick(32, 0) //Space

            //Make sure we have the repository and it's created
            let repoListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->repositoryListView")
            compare(repoListView.count, 1)
            waitForRendering(repoListView);

            let firstDelegate = repoListView.itemAtIndex(0);
            compare(firstDelegate.nameRole, "test")

            //Try to add the test directory again
            mouseClick(addButton)
            tryVerify(() => { return findChild(mainWindow, "addMenuNew") !== null })
            addMenuNew = findChild(mainWindow, "addMenuNew")
            mouseClick(addMenuNew)

            cavingAreaNameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "Popup->cavingAreaName")
            compare(cavingAreaNameTextEdit.textField.text, "")
            compare(cavingAreaNameTextEdit.hasError, false)

            //Write test
            nameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "Popup->cavingAreaName->TextField")
            mouseClick(nameTextEdit)

            //Write test
            keyClick("t")
            keyClick("e")
            keyClick("s")

            //Write test
            let errorArea = findChild(mainWindow, "newRepoErrorArea")
            waitForRendering(errorArea);
            compare(errorArea.visible, false)
            compare(errorArea.text, "")

            //Make sure there's no errors
            compare(cavingAreaNameTextEdit.hasError, false)
            compare(cavingAreaNameTextEdit.errorMessage, "")

            keyClick("t")

            compare(errorArea.visible, false)
            compare(errorArea.text, "")

            //Make sure there's an error about the directory exists
            compare(cavingAreaNameTextEdit.hasError, false)
            verify(cavingAreaNameTextEdit.errorMessage.includes("exists, use a different name"))

            //User clicks okay
            let openRepoButton = ObjectFinder.findObjectByChain(mainWindow, "Popup->openRepoButton")
            mouseClick(openRepoButton)

            //Make sure there's an error about the directory exists
            compare(cavingAreaNameTextEdit.hasError, true)
            verify(cavingAreaNameTextEdit.errorMessage.includes("exists, use a different name"))
            compare(errorArea.visible, false)

            mouseClick(nameTextEdit)
            keyClick("2")

            compare(cavingAreaNameTextEdit.hasError, false)
            compare(errorArea.visible, false)

            //Cancel
            let cancelRepoButton = ObjectFinder.findObjectByChain(mainWindow, "Popup->cancelRepoButton")
            mouseClick(cancelRepoButton)
        }

        function test_libgit2Error() {
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow);

            let addButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->addRepositoryButton")
            mouseClick(addButton)
            tryVerify(() => { return findChild(mainWindow, "addMenuNew") !== null })
            let addMenuNew = findChild(mainWindow, "addMenuNew")
            mouseClick(addMenuNew)

            //We're going to assume we don't have permission here and trying to add repo at root will throw an error
            let dir = TestHelper.toLocalUrl("/");
            RootData.recentProjectModel.defaultRepositoryDir = TestHelper.toLocalUrl("/")

            let dirText = ObjectFinder.findObjectByChain(mainWindow, "Popup->repositoryParentDir")
            compare(dirText.text, RootData.urlToLocal(RootData.recentProjectModel.defaultRepositoryDir))
            compare(dirText.text, RootData.urlToLocal(dir))

            //Write test
            let nameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "Popup->cavingAreaName->TextField")
            mouseClick(nameTextEdit)

            //Write test
            keyClick("t")
            keyClick("e")
            keyClick("s")
            keyClick("t")

            let cavingAreaNameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "Popup->cavingAreaName")
            let errorArea = ObjectFinder.findObjectByChain(mainWindow, "Popup->newRepoErrorArea")
            waitForRendering(errorArea);

            compare(cavingAreaNameTextEdit.hasError, false)
            compare(errorArea.visible, false)

            //User clicks okay
            let openRepoButton = ObjectFinder.findObjectByChain(mainWindow, "Popup->openRepoButton")
            mouseClick(openRepoButton)

            compare(cavingAreaNameTextEdit.hasError, false)
            compare(errorArea.visible, true) //The git error should be displayed here
            verify(errorArea.text.includes("failed to make directory"));

            //Cancel
            let cancelRepoButton = ObjectFinder.findObjectByChain(mainWindow, "Popup->cancelRepoButton")
            mouseClick(cancelRepoButton)

            //Make sure the dialog goes away
            let loader = ObjectFinder.findObjectByChain(mainWindow, "rootId->whereDialogLoader");
            compare(loader.active, false)
        }

        function test_openV6CavewhereFile() {
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow);

            //Copy the test project to the test folder
            let filename = TestHelper.copyToTempDir("://datasets/test_cwProject/Phake Cave 3000.cw");

            // let openCavingAreaButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->openCavingAreaButton")
            // mouseClick(openCavingAreaButton_obj1, 61.707, 18.5938)

            console.log("Temp file:" + filename)

            //Simulating opening a file
            let dialog = ObjectFinder.findObjectByChain(mainWindow, "rootId->loadProjectDialog")
            dialog.runLoadForTest(TestHelper.toLocalUrl(filename))

            //Make we have all the data
            tryVerify(() => { return RootData.region.caveCount > 0 }, 20000)
            tryCompare(RootData.project, "fileType", Project.GitFileType, 5000)

            verify(RootData.project.save())
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            TestHelper.waitForFutureManagerToFinish(RootData.futureManagerModel)

            compare(RootData.project.fileType, Project.GitFileType)

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

            let filename = TestHelper.copyToTempDir("://datasets/test_cwProject/Phake Cave 3000.cw");
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

            let filename = TestHelper.copyToTempDir("://datasets/test_cwProject/Phake Cave 3000.cw");
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
