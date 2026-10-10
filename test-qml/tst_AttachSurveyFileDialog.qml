import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// C7.4a: the Data page's Add ▾ → Attach survey file… and the
// AttachSurveyFileDialog it opens - the summary line, "Place under", the
// subtree preview with its gray rows, the happy path (a source root under the
// target, named from the file, with the file's blocks as nodes), Attach under
// a Folder through a row's Add submenu, Import a copy, and a dismissal that
// leaves the project as it was.
MainWindowTest {
    id: rootId

    ExternalCenterlineTestCase {
        name: "AttachSurveyFileDialog"
        when: windowShown

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function cleanup() {
            // A dialog left open by a failed assertion is modal: its
            // overlay would eat the next test's clicks.
            const dialog = findChild(mainWindow, "attachSurveyFileDialog")
            if (dialog !== null) {
                dialog.close()
            }
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
        }

        function gotoDataMainPage() {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      10000, "data page opens")
            // The action bar is placed via LayoutItemProxy; clicks miss
            // until the proxied layout settles.
            waitForRendering(rootId)
            return RootData.pageView.currentPageItem
        }

        // Opens the split button's chevron menu on the data page and
        // returns its Attach survey file… item.
        function openAddCaveMenu(dataPage) {
            const addBar = findChild(dataPage, "addCave")
            verify(addBar !== null, "addCave bar must exist")
            const menuButton = findChild(addBar, "menuButton")
            verify(menuButton !== null, "split-button chevron must exist")
            mouseClick(menuButton)

            const menu = findChild(dataPage, "addCaveMenu")
            verify(menu !== null, "addCaveMenu must exist")
            tryVerify(() => menu.opened, 5000, "menu opens on chevron click")
            const item = findChild(menu, "attachSurveyFileMenuItem")
            verify(item !== null, "the menu offers Attach survey file…")
            return item
        }

        // The page's dialog once it is open, ready for a path to be typed
        // into its field.
        function openedDialog(page) {
            let dialog = null
            tryVerify(() => {
                          dialog = findChild(page, "attachSurveyFileDialog")
                          return dialog !== null
                      }, 5000, "the dialog must exist on the page")
            const pathField = findChild(dialog, "sourcePathField")
            verify(pathField !== null, "sourcePathField must exist")
            tryVerify(() => pathField.visible, 5000, "dialog opens")
            return dialog
        }

        // Runs the menu item and returns the opened dialog.
        function openDialog(dataPage) {
            mouseClick(openAddCaveMenu(dataPage))
            return openedDialog(dataPage)
        }

        // Points the dialog's picker at a fixture and waits for the
        // scan behind Attach to land.
        function pickFixture(dialog, fixture) {
            const pathField = findChild(dialog, "sourcePathField")
            // In-source path, no copy: a master file has to be read
            // where the files it includes are.
            pathField.text = TestHelper.testcasesDatasetSourcePath(
                "external-centerlines/" + fixture)
            const attachButton = findChild(dialog, "attachButton")
            tryVerify(() => attachButton.enabled, 10000, "preview scan lands")
            return attachButton
        }

        function cancelDialog(dialog) {
            const cancelButton = findChild(dialog, "cancelButton")
            waitForRendering(rootId)
            mouseClick(cancelButton)
            RootData.futureManagerModel.waitForFinished()
        }

        function test_menuItemPresent() {
            saveProjectAs("attach-menu")
            const dataPage = gotoDataMainPage()

            const item = openAddCaveMenu(dataPage)
            compare(item.text, "Attach survey file…")
        }

        function test_happyPathBuildsTheSubtreeUnderTheRegion() {
            saveProjectAs("attach-happy")
            const dataPage = gotoDataMainPage()
            compare(RootData.region.rowCount(), 0)

            const dialog = openDialog(dataPage)
            compare(dialog.target, RootData.region.rootNode,
                    "Attach builds under the region's root")
            compare(findChild(dialog, "placeUnderText").text, "Region root (top level)")
            compare(RootData.region.rowCount(), 0,
                    "opening the dialog creates nothing")

            const attachButton = pickFixture(dialog, "survex_blocks.svx")

            const preview = findChild(dialog, "blockPreview")
            verify(preview !== null, "block preview must exist")
            tryVerify(() => preview.visible, 5000, "the preview shows the subtree")
            compare(findChild(dialog, "blockPreviewTitle").text, "Subtree CaveWhere will create")

            waitForRendering(rootId)
            mouseClick(attachButton)

            tryVerify(() => RootData.region.rowCount() === 1, 10000,
                      "the attach adds one source root")
            // The root appears when the scan lands; its file and subtree once
            // the copy is in the project.
            const sourceRoot = RootData.region.rootNode.childNode(0)
            compare(sourceRoot.name, "survex_blocks")
            tryVerify(() => sourceRoot.externalCenterline.entryFile === "survex_blocks.svx",
                      10000, "the root becomes attached")
            verify(sourceRoot.isSourceRoot, "the new node is the file's source root")
            compare(sourceRoot.kind, SurveyNodeKind.SurvexFile)
            compare(sourceRoot.childNodeCount, 1, "doghill is the file's one top-level block")
            const doghill = sourceRoot.childNode(0)
            compare(doghill.name, "doghill")
            compare(doghill.kind, SurveyNodeKind.SurvexBlock)
            compare(doghill.tripCount, 1, "doghill's own shots are one derived trip")

            // The Data page stays in front with the new root shown open.
            compare(RootData.pageView.currentPageItem, dataPage)
            const tree = findChild(dataPage, "caveDelegate0").treeView
            tryVerify(() => tree.surveyTree.currentObject === sourceRoot, 10000,
                      "the new root's row is current")
            tryVerify(() => tree.isExpanded(0), 10000, "the new root shows its blocks")
            RootData.futureManagerModel.waitForFinished()
        }

        // The summary line counts what the scan found and names the listed
        // file the disk lacks.
        function test_summaryCountsFilesSurveysAndMissingFiles() {
            saveProjectAs("attach-summary")
            const dataPage = gotoDataMainPage()
            const dialog = openDialog(dataPage)

            pickFixture(dialog, "compass_project/PROJECT.MAK")
            const summary = findChild(dialog, "scanSummaryText")
            tryVerify(() => summary.visible, 10000, "the summary shows once the scan lands")
            compare(summary.text,
                    "✓ Compass project — 4 files, 4 surveys, 14 stations · 1 listed file missing")
            compare(findChild(dialog, "blockPreviewHint").text,
                    "One node per .dat file, one trip per survey.")

            cancelDialog(dialog)
        }

        // The preview row the list draws for \a name, once the scan lands.
        function previewRow(dialog, name) {
            const list = findChild(dialog, "blockPreviewList")
            verify(list !== null, "blockPreviewList must exist")
            let row = null
            tryVerify(() => {
                for (let i = 0; i < list.count; ++i) {
                    const item = list.itemAtIndex(i)
                    if (item !== null && item.modelData.displayName === name) {
                        row = item
                        return true
                    }
                }
                return false
            }, 10000, "the preview lists " + name)
            return row
        }

        // Rows name their kind and station count, and gray out a book with no
        // survey data under it and a file the disk lacks.
        function test_previewGraysEmptyBooksAndMissingFiles() {
            saveProjectAs("attach-preview-rows")
            const dataPage = gotoDataMainPage()
            const dialog = openDialog(dataPage)

            pickFixture(dialog, "walls_project/KAUA.WPJ")
            const drawings = previewRow(dialog, "Drawings")
            verify(drawings.grayed, "a book holding no survey file is gray")
            compare(drawings.color, Theme.textSubtle)
            verify(drawings.text.startsWith("Drawings · Walls book — 0 station"), drawings.text)

            const surveys = previewRow(dialog, "Surveys")
            verify(!surveys.grayed, "a book with surveys under it reads normally")
            compare(surveys.color, Theme.text)
            verify(surveys.text.startsWith("Surveys · Walls book — 8 station"), surveys.text)

            const unit = previewRow(dialog, "N Surveys")
            verify(unit.text.startsWith("N Surveys · Trip — 4 station"), unit.text)

            pickFixture(dialog, "compass_project/PROJECT.MAK")
            const delta = previewRow(dialog, "Delta")
            verify(delta.grayed, "an absent file is gray")
            compare(delta.color, Theme.textSubtle)
            compare(delta.text, "Delta · Compass file — missing")

            const alpha = previewRow(dialog, "Alpha")
            verify(!alpha.grayed, "a present file with surveys reads normally")
            verify(alpha.text.startsWith("Alpha · Compass file — 7 station"), alpha.text)

            cancelDialog(dialog)
        }

        // A row's Add submenu opens the dialog with that row's node as the
        // place, and the attach lands there.
        function test_attachUnderAFolderLandsUnderIt() {
            saveProjectAs("attach-under-folder")
            const folder = RootData.region.addNode(null, SurveyNodeKind.Folder, "Kentucky")
            const dataPage = gotoDataMainPage()

            let row = null
            tryVerify(() => {
                          row = findChild(dataPage, "caveDelegate0")
                          return row !== null && row.object === folder
                      }, 5000, "the Folder's row shows")
            const tree = row.treeView
            tree.surveyTree.setCurrentRow(0)
            tree.forceActiveFocus()
            tryVerify(() => tree.activeFocus && tree.currentRow === 0, 5000)
            keyClick(Qt.Key_F10, Qt.ShiftModifier)

            // The menu belongs to the row, so it is found under the row.
            let menu = null
            tryVerify(() => {
                          menu = findChild(row, "surveyItemContextMenu")
                          return menu !== null && menu.visible
                      }, 5000, "the row's menu opens")
            const submenu = menu.menuAt(0)
            compare(submenu.objectName, "addVerbsSubmenu")
            mouseClick(menu.itemAt(0))
            tryVerify(() => submenu.opened, 5000, "Add opens its verbs")
            mouseClick(findChild(submenu, "attachSurveyFileMenuItem"))

            const dialog = openedDialog(dataPage)
            compare(dialog.target, folder, "the dialog places the file under the row's node")
            compare(findChild(dialog, "placeUnderText").text, "Kentucky (Folder)")

            const attachButton = pickFixture(dialog, "survex_master/master.svx")
            waitForRendering(rootId)
            mouseClick(attachButton)

            tryVerify(() => folder.childNodeCount === 1, 10000, "the root lands under the Folder")
            const sourceRoot = folder.childNode(0)
            tryVerify(() => sourceRoot.isSourceRoot && sourceRoot.childNodeCount === 3, 10000,
                      "the Survex master's three blocks land as nodes")
            compare(RootData.region.rowCount(), 1, "nothing lands at the top level")
            tryVerify(() => tree.surveyTree.currentObject === sourceRoot, 10000,
                      "the new root's row is current")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_importNamesCaveAndTripAfterTheFile() {
            saveProjectAs("attach-import")
            const dataPage = gotoDataMainPage()
            const dialog = openDialog(dataPage)

            const pathField = findChild(dialog, "sourcePathField")
            pathField.text = TestHelper.testcasesDatasetSourcePath(
                "external-centerlines/survex_simple.svx")
            const importButton = findChild(dialog, "importButton")
            verify(importButton !== null, "importButton must exist")
            tryVerify(() => importButton.enabled, 10000,
                      "Import enables on a valid Survex scan")

            importButton.clicked()
            verify(dialog.importing, "import busy state is up")
            const newCave = dialog.importedCave
            verify(newCave !== null, "import adds a cave of its own")
            compare(newCave.parentNode, RootData.region.rootNode, "the cave goes where the dialog places it")
            const cancelButton = findChild(dialog, "cancelButton")
            verify(!cancelButton.enabled, "no cancel path during import")

            tryVerify(() => newCave.rowCount() === 1
                            && newCave.trip(0).chunkCount > 0,
                      10000, "the import lands chunks in one new trip")
            compare(newCave.trip(0).externalCenterline.entryFile, "",
                    "an imported trip stays Native")
            tryCompare(newCave.trip(0), "name", "survex_simple")
            tryCompare(newCave, "name", "survex_simple")
            compare(newCave.externalCenterline.entryFile, "",
                    "an imported cave stays Native")
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      10000, "import navigates to the new cave's page")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_dismissLeavesTheProjectAsItWas() {
            saveProjectAs("attach-dismiss")
            const dataPage = gotoDataMainPage()
            const rowsBefore = RootData.region.rowCount()

            const dialog = openDialog(dataPage)
            compare(RootData.region.rowCount(), rowsBefore,
                    "opening the dialog creates nothing")

            const cancelButton = findChild(dialog, "cancelButton")
            verify(cancelButton !== null, "dialog cancel button must exist")
            waitForRendering(rootId)
            mouseClick(cancelButton)

            tryVerify(() => !findChild(dialog, "sourcePathField").visible, 5000,
                      "the dialog closes")
            compare(RootData.region.rowCount(), rowsBefore)
            RootData.futureManagerModel.waitForFinished()
        }
    }
}
