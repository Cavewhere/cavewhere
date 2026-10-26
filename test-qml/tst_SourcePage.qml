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

        // Copies the Phake Cave dataset to each name in the temp dir and returns the paths.
        function copyProjects(names) {
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            const source = TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw")
            return names.map((name) => {
                const path = tempDir + "/" + name + ".cw"
                verify(TestHelper.copyFile(source, path), "copy to " + path)
                return path
            })
        }

        // Stamps each path in order, letting the clock tick between stamps so
        // every entry gets a distinct Last opened time.
        function recordOpenedInOrder(paths) {
            for (const path of paths) {
                const before = Date.now()
                tryVerify(() => Date.now() > before)
                const result = RootData.recentProjectModel.recordProjectOpened(TestHelper.toLocalUrl(path))
                verify(!result.hasError, result.errorMessage)
            }
        }

        function gotoSourcePage() {
            RootData.pageSelectionModel.currentPageAddress = "Source"
            waitForRendering(mainWindow)
            let repoListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->repositoryListView")
            verify(repoListView !== null, "repositoryListView not found")
            return repoListView
        }

        function typeIntoField(field, text) {
            mouseClick(field)
            for (const character of text) {
                keyClick(character)
            }
        }

        // The page persists between tests and keeps its last sort, so put the
        // newest-first default back before the next test.
        function restoreDefaultSort() {
            let lastOpenedHeader = findChild(mainWindow, "recentHeaderLastOpened")
            verify(lastOpenedHeader !== null, "recentHeaderLastOpened not found")
            mouseClick(lastOpenedHeader)
            mouseClick(lastOpenedHeader)
            tryCompare(lastOpenedHeader, "sortMode", TableStaticHeaderColumn.Sort.Descending)
        }

        function clearFilterField(field) {
            mouseClick(field)
            keyClick(Qt.Key_A, Qt.ControlModifier)
            keyClick(Qt.Key_Delete)
            tryCompare(field, "text", "")
        }

        function rowPath(listView, row) {
            const item = listView.itemAtIndex(row)
            return item === null ? "" : item.pathRole
        }

        function test_defaultOrder_mostRecentFirst() {
            const paths = copyProjects(["source-order-a", "source-order-b"])
            recordOpenedInOrder(paths)

            let repoListView = gotoSourcePage()
            tryCompare(repoListView, "count", 2)
            tryVerify(() => rowPath(repoListView, 0) === paths[1], 5000, "most recently opened should be first")
            tryVerify(() => rowPath(repoListView, 1) === paths[0])

            let lastOpenedLabel = findChild(mainWindow, "repoLastOpened_0")
            verify(lastOpenedLabel !== null, "repoLastOpened_0 not found")
            verify(/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}$/.test(lastOpenedLabel.text), "unexpected date text: " + lastOpenedLabel.text)
        }

        function test_headerClick_sortsByName() {
            const paths = copyProjects(["source-sort-bravo", "source-sort-alpha"])
            recordOpenedInOrder(paths)

            let repoListView = gotoSourcePage()
            tryCompare(repoListView, "count", 2)

            let nameHeader = findChild(mainWindow, "recentHeaderName")
            verify(nameHeader !== null, "recentHeaderName not found")
            let lastOpenedHeader = findChild(mainWindow, "recentHeaderLastOpened")
            verify(lastOpenedHeader !== null, "recentHeaderLastOpened not found")
            compare(lastOpenedHeader.sortMode, TableStaticHeaderColumn.Sort.Descending)

            mouseClick(nameHeader)
            tryVerify(() => rowPath(repoListView, 0) === paths[1], 5000, "alpha should sort first")
            tryVerify(() => rowPath(repoListView, 1) === paths[0])
            tryCompare(lastOpenedHeader, "sortMode", TableStaticHeaderColumn.Sort.None)

            mouseClick(nameHeader)
            tryVerify(() => rowPath(repoListView, 0) === paths[0], 5000, "bravo should sort first when reversed")
            tryVerify(() => rowPath(repoListView, 1) === paths[1])

            restoreDefaultSort()
        }

        function test_filter_narrowsList() {
            const paths = copyProjects(["source-filter-alpha", "source-filter-bravo"])
            recordOpenedInOrder(paths)

            let repoListView = gotoSourcePage()
            tryCompare(repoListView, "count", 2)

            let filterField = findChild(mainWindow, "recentProjectFilterField")
            verify(filterField !== null, "recentProjectFilterField not found")

            typeIntoField(filterField, "brav")
            tryCompare(repoListView, "count", 1)
            tryVerify(() => rowPath(repoListView, 0) === paths[1], 5000, "bravo should be the only match")

            clearFilterField(filterField)
            tryCompare(repoListView, "count", 2)

            typeIntoField(filterField, "zzq")
            tryCompare(repoListView, "count", 0)
            let emptyLabel = findChild(mainWindow, "recentProjectsEmptyLabel")
            verify(emptyLabel !== null, "recentProjectsEmptyLabel not found")
            tryCompare(emptyLabel, "visible", true)
            tryCompare(emptyLabel, "text", "No projects match the filter.")

            clearFilterField(filterField)
            tryCompare(repoListView, "count", 2)
        }

        function test_clickRow_afterSort_opensThatProject() {
            // Source order is bravo, alpha; sorting by name puts alpha (source row 1) first.
            const paths = copyProjects(["source-click-bravo", "source-click-alpha"])
            recordOpenedInOrder(paths)

            let repoListView = gotoSourcePage()
            tryCompare(repoListView, "count", 2)

            let nameHeader = findChild(mainWindow, "recentHeaderName")
            verify(nameHeader !== null, "recentHeaderName not found")
            mouseClick(nameHeader)
            tryVerify(() => rowPath(repoListView, 0) === paths[1], 5000, "alpha should sort first")
            waitForRendering(repoListView)

            let linkText = findChild(mainWindow, "repoLinkText_0")
            verify(linkText !== null, "repoLinkText_0 not found")
            compare(linkText.text, "source-click-alpha.cw")
            mouseClick(linkText)

            let askToSaveDialog = findChild(rootId, "mainWindowTestAskToSaveDialog")
            if (askToSaveDialog !== null
                    && askToSaveDialog._dialog !== null
                    && askToSaveDialog._dialog.askToSaveDialog.visible) {
                askToSaveDialog._dialog.askToSaveDialog.discarded()
            }

            tryCompare(RootData.project, "filename", paths[1], 20000)
            tryVerify(() => RootData.region.caveCount > 0, 20000)

            // Let the legacy-to-bundle conversion settle so later tests start clean.
            tryCompare(RootData.project, "fileType", Project.BundledGitFileType, 5000)
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            TestHelper.waitForFutureManagerToFinish(RootData.futureManagerModel)
            RootData.newProject()

            gotoSourcePage()
            restoreDefaultSort()
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
            // A .cwproj save commits, and the commit needs an account.
            RootData.account.name = "Recent Test"
            RootData.account.email = "recent.test@example.com"
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
