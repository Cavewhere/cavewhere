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
            RootData.repositoryModel.clear();
        }

        function test_addRepository() {
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow);

            let label_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->newCavingAreaButton->label")
            mouseClick(label_obj1)

            let tempDir = TestHelper.tempDirectoryUrl();
            RootData.repositoryModel.defaultRepositoryDir = tempDir

            let dirText = ObjectFinder.findObjectByChain(mainWindow, "repositoryParentDir")
            compare(dirText.text, RootData.urlToLocal(RootData.repositoryModel.defaultRepositoryDir))
            compare(dirText.text, RootData.urlToLocal(tempDir))

            //Write test
            let nameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "cavingAreaName->TextField")
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

            let label_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->newCavingAreaButton->label")
            mouseClick(label_obj1)

            let tempDir = TestHelper.tempDirectoryUrl();
            RootData.repositoryModel.defaultRepositoryDir = tempDir

            let dirText = ObjectFinder.findObjectByChain(mainWindow, "repositoryParentDir")
            compare(dirText.text, RootData.urlToLocal(RootData.repositoryModel.defaultRepositoryDir))
            compare(dirText.text, RootData.urlToLocal(tempDir))

            //Make sure the error isn't visible yet
            let cavingAreaNameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "cavingAreaName")
            compare(cavingAreaNameTextEdit.textField.text, "")
            compare(cavingAreaNameTextEdit.hasError, false)


            //User clicks okay
            let label_obj2 = ObjectFinder.findObjectByChain(mainWindow, "openRepoButton")
            mouseClick(label_obj2)

            //Error should be shown
            compare(cavingAreaNameTextEdit.hasError, true)
            compare(cavingAreaNameTextEdit.errorMessage, "Caving area name is empty")

            //Write test
            let nameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "cavingAreaName->TextField")
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
            mouseClick(label_obj1)

            cavingAreaNameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "cavingAreaName")
            compare(cavingAreaNameTextEdit.textField.text, "")
            compare(cavingAreaNameTextEdit.hasError, false)

            //Write test
            nameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "cavingAreaName->TextField")
            mouseClick(nameTextEdit)

            //Write test
            keyClick("t")
            keyClick("e")
            keyClick("s")

            //Write test
            let errorArea = ObjectFinder.findObjectByChain(mainWindow, "newRepoErrorArea")
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
            let openRepoButton = ObjectFinder.findObjectByChain(mainWindow, "openRepoButton")
            mouseClick(openRepoButton)

            //Make sure there's an error about the directory exists
            compare(cavingAreaNameTextEdit.hasError, true)
            verify(cavingAreaNameTextEdit.errorMessage.includes("exists, use a different name"))
            compare(errorArea.visible, false)

            keyClick("2")

            compare(cavingAreaNameTextEdit.hasError, false)
            compare(errorArea.visible, false)

            //Cancel
            let cancelRepoButton = ObjectFinder.findObjectByChain(mainWindow, "cancelRepoButton")
            mouseClick(cancelRepoButton)
        }

        function test_libgit2Error() {
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow);

            let label_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->newCavingAreaButton->label")
            mouseClick(label_obj1)

            //We're going to assume we don't have permission here and trying to add repo at root will throw an error
            let dir = TestHelper.toLocalUrl("/");
            RootData.repositoryModel.defaultRepositoryDir = TestHelper.toLocalUrl("/")

            let dirText = ObjectFinder.findObjectByChain(mainWindow, "repositoryParentDir")
            compare(dirText.text, RootData.urlToLocal(RootData.repositoryModel.defaultRepositoryDir))
            compare(dirText.text, RootData.urlToLocal(dir))

            //Write test
            let nameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "cavingAreaName->TextField")
            mouseClick(nameTextEdit)

            //Write test
            keyClick("t")
            keyClick("e")
            keyClick("s")
            keyClick("t")

            let cavingAreaNameTextEdit = ObjectFinder.findObjectByChain(mainWindow, "cavingAreaName")
            let errorArea = ObjectFinder.findObjectByChain(mainWindow, "newRepoErrorArea")
            waitForRendering(errorArea);

            compare(cavingAreaNameTextEdit.hasError, false)
            compare(errorArea.visible, false)

            //User clicks okay
            let openRepoButton = ObjectFinder.findObjectByChain(mainWindow, "openRepoButton")
            mouseClick(openRepoButton)

            compare(cavingAreaNameTextEdit.hasError, false)
            compare(errorArea.visible, true) //The git error should be displayed here
            verify(errorArea.text.includes("failed to make directory"));

            //Cancel
            let cancelRepoButton = ObjectFinder.findObjectByChain(mainWindow, "cancelRepoButton")
            mouseClick(cancelRepoButton)

            //Make sure the dialog goes away
            let loader = ObjectFinder.findObjectByChain(mainWindow, "rootId->whereDialogLoader");
            compare(loader.active, false)
        }
    }
}
