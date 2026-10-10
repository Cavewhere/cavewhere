import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// P3.7 tests: DataMainPage's [Add Cave] split-button menu and the
// AddCaveSurveyFileDialog it opens - the block preview, the multi-cave
// heuristic and its one-click alternate targets, the happy path
// (a source root under the region, named from the file, with the file's
// blocks as nodes) and a dismissal that leaves the project as it was.
MainWindowTest {
    id: rootId

    ExternalCenterlineTestCase {
        name: "AddCaveFromSurveyFile"
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
            const dialog = findChild(mainWindow, "addCaveSurveyFileDialog")
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
        // returns its Add from Survey File… item.
        function openAddCaveMenu(dataPage) {
            const addBar = findChild(dataPage, "addCave")
            verify(addBar !== null, "addCave bar must exist")
            const menuButton = findChild(addBar, "menuButton")
            verify(menuButton !== null, "split-button chevron must exist")
            mouseClick(menuButton)

            const menu = findChild(dataPage, "addCaveMenu")
            verify(menu !== null, "addCaveMenu must exist")
            tryVerify(() => menu.visible, 5000, "menu opens on chevron click")
            const item = findChild(menu, "addExternalCaveMenuItem")
            verify(item !== null, "the menu offers Add from Survey File…")
            return item
        }

        // Runs the menu item and returns the opened dialog, ready for a
        // path to be typed into its field.
        function openDialog(dataPage) {
            mouseClick(openAddCaveMenu(dataPage))

            const dialog = findChild(dataPage, "addCaveSurveyFileDialog")
            verify(dialog !== null, "the dialog must exist on the data page")
            const pathField = findChild(dialog, "sourcePathField")
            verify(pathField !== null, "sourcePathField must exist")
            tryVerify(() => pathField.visible, 5000, "dialog opens")
            return dialog
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

        function test_menuItemPresent() {
            saveProjectAs("add-cave-menu")
            const dataPage = gotoDataMainPage()

            const item = openAddCaveMenu(dataPage)
            compare(item.text, qsTr("Add from Survey File…"))
        }

        function test_happyPathBuildsTheSubtreeUnderTheRegion() {
            saveProjectAs("add-cave-happy")
            const dataPage = gotoDataMainPage()
            compare(RootData.region.rowCount(), 0)

            const dialog = openDialog(dataPage)
            compare(dialog.target, RootData.region.rootNode,
                    "Attach builds under the region's root")
            compare(RootData.region.rowCount(), 0,
                    "opening the dialog creates nothing")

            const attachButton = pickFixture(dialog, "survex_blocks.svx")

            // Every block is previewed, sump included - shown so the
            // structure reads whole (§5 Q3).
            const preview = findChild(dialog, "blockPreview")
            verify(preview !== null, "block preview must exist")
            tryVerify(() => preview.visible, 5000, "the preview shows the block tree")

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

            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      10000, "attach navigates to the new root's page")
            RootData.futureManagerModel.waitForFinished()
        }

        // P3.15: the stations no block claims get one whole-file trip,
        // so a Compass project explains its single trip before Attach.
        function test_wholeFileRowShowsForRootStationsOnly() {
            saveProjectAs("add-cave-whole-file")
            const dataPage = gotoDataMainPage()
            const dialog = openDialog(dataPage)

            pickFixture(dialog, "compass_multi.mak")
            const wholeFileRow = findChild(dialog, "wholeFileRow")
            verify(wholeFileRow !== null, "the whole-file row must exist")
            tryVerify(() => wholeFileRow.visible, 10000,
                      "a Compass project's stations all sit in the file root")
            verify(wholeFileRow.text.indexOf("5") >= 0,
                   "the row names the root station count; got: " + wholeFileRow.text)
            // The tree lists the project root, its two DATs, and one
            // survey under each.
            const list = findChild(dialog, "blockPreviewList")
            tryCompare(list, "count", 5, 5000)

            // Every station of a *begin tree belongs to a block, so
            // there is nothing left for a whole-file trip to own.
            pickFixture(dialog, "survex_blocks.svx")
            tryVerify(() => !wholeFileRow.visible, 10000,
                      "a blocks-only file shows no whole-file row")

            const cancelButton = findChild(dialog, "cancelButton")
            waitForRendering(rootId)
            mouseClick(cancelButton)
            RootData.futureManagerModel.waitForFinished()
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

        // C7.1: rows name their kind and station count, and gray out a
        // book with no survey data under it and a file the disk lacks.
        function test_previewGraysEmptyBooksAndMissingFiles() {
            saveProjectAs("add-cave-preview-rows")
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

            const cancelButton = findChild(dialog, "cancelButton")
            waitForRendering(rootId)
            mouseClick(cancelButton)
            RootData.futureManagerModel.waitForFinished()
        }

        function test_importNamesCaveAndTripAfterTheFile() {
            saveProjectAs("add-cave-import")
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
            saveProjectAs("add-cave-dismiss")
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

        function test_heuristicShowsForMasterOnly() {
            saveProjectAs("add-cave-heuristic")
            const dataPage = gotoDataMainPage()
            const dialog = openDialog(dataPage)

            const panel = findChild(dialog, "multiCaveHeuristicPanel")
            verify(panel !== null, "heuristic panel must exist")
            verify(!panel.visible, "nothing scanned yet, nothing to warn about")

            const attachButton = pickFixture(dialog, "survex_master/master.svx")
            tryVerify(() => panel.visible, 10000,
                      "a project master trips the heuristic")
            const body = findChild(dialog, "multiCaveHeuristicBody")
            verify(body.text.indexOf("3") >= 0,
                   "the body names the top-level block count; got: " + body.text)
            verify(attachButton.enabled,
                   "the heuristic is guidance - Attach stays enabled (§5 Q4)")

            pickFixture(dialog, "survex_blocks.svx")
            tryVerify(() => !panel.visible, 10000,
                      "a single-cave file does not trip the heuristic")

            const cancelButton = findChild(dialog, "cancelButton")
            waitForRendering(rootId)
            mouseClick(cancelButton)
            RootData.futureManagerModel.waitForFinished()
        }

        function test_suggestionClickRepointsPicker() {
            saveProjectAs("add-cave-suggestion")
            const dataPage = gotoDataMainPage()
            const dialog = openDialog(dataPage)

            pickFixture(dialog, "survex_master/master.svx")
            const panel = findChild(dialog, "multiCaveHeuristicPanel")
            tryVerify(() => panel.visible, 10000, "the heuristic shows")

            // Through the Repeater, not findChild: delegate items are
            // parented into the layout, where findChild misses them.
            const suggestions = findChild(dialog, "heuristicSuggestions")
            verify(suggestions !== null, "the panel repeats the entry's direct includes")
            tryVerify(() => suggestions.count === 3, 5000,
                      "one suggestion per direct include")
            const suggestion = suggestions.itemAt(0)
            compare(suggestion.objectName, "heuristicSuggestion")
            compare(suggestion.text, "alpha.svx",
                    "suggestions read as file names, not full paths")

            waitForRendering(rootId)
            mouseClick(suggestion)

            const pathField = findChild(dialog, "sourcePathField")
            tryVerify(() => pathField.text.endsWith("alpha.svx"), 5000,
                      "clicking a suggestion re-points the picker")
            tryVerify(() => !panel.visible, 10000,
                      "the rescanned single-cave file retires the panel")

            const cancelButton = findChild(dialog, "cancelButton")
            waitForRendering(rootId)
            mouseClick(cancelButton)
            RootData.futureManagerModel.waitForFinished()
        }
    }
}
