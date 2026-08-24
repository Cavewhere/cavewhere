import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// P3.7 tests: DataMainPage's [Add Cave] split-button menu and the
// AddCaveSurveyFileDialog it opens - the block preview, the multi-cave
// heuristic and its one-click alternate targets, the happy path
// (cave named from the file, one Scope trip per station-bearing block)
// and the dismissal that deletes the cave the flow created up front.
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
        // returns its one item.
        function openAddCaveMenu(dataPage) {
            const addBar = findChild(dataPage, "addCave")
            verify(addBar !== null, "addCave bar must exist")
            const menuButton = findChild(addBar, "menuButton")
            verify(menuButton !== null, "split-button chevron must exist")
            mouseClick(menuButton)

            const menu = findChild(dataPage, "addCaveMenu")
            verify(menu !== null, "addCaveMenu must exist")
            tryVerify(() => menu.visible, 5000, "menu opens on chevron click")
            compare(menu.count, 1, "menu shows exactly one item")
            const item = menu.itemAt(0)
            compare(item.objectName, "addExternalCaveMenuItem")
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
            compare(item.text, qsTr("Add cave from survey file…"))
        }

        function test_happyPathCreatesNamedCaveWithScopeTrips() {
            saveProjectAs("add-cave-happy")
            const dataPage = gotoDataMainPage()
            compare(RootData.region.rowCount(), 0)

            const dialog = openDialog(dataPage)
            compare(RootData.region.rowCount(), 1,
                    "the flow creates the cave up front")

            const attachButton = pickFixture(dialog, "survex_blocks.svx")

            // Every station-bearing block is previewed, sump included -
            // shown so the structure reads whole (§5 Q3).
            const preview = findChild(dialog, "blockPreview")
            verify(preview !== null, "block preview must exist")
            tryVerify(() => preview.visible, 5000, "the preview shows the block tree")
            const hint = findChild(dialog, "blockPreviewHint")
            verify(hint.text.indexOf("One trip per") === 0,
                   "the preview explains the rule; got: " + hint.text)

            waitForRendering(rootId)
            mouseClick(attachButton)

            const newCave = RootData.region.cave(0)
            tryVerify(() => newCave.externalCenterline.entryFile === "survex_blocks.svx",
                      10000, "the cave becomes Attached")
            tryCompare(newCave, "name", "survex_blocks")
            tryVerify(() => newCave.rowCount() === 3, 10000,
                      "one Scope trip per station-bearing block")

            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      10000, "attach navigates to the new cave's page")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_dismissRemovesTheJustCreatedCave() {
            saveProjectAs("add-cave-dismiss")
            const dataPage = gotoDataMainPage()
            const rowsBefore = RootData.region.rowCount()

            const dialog = openDialog(dataPage)
            compare(RootData.region.rowCount(), rowsBefore + 1,
                    "the flow creates the cave up front")

            const cancelButton = findChild(dialog, "cancelButton")
            verify(cancelButton !== null, "dialog cancel button must exist")
            waitForRendering(rootId)
            mouseClick(cancelButton)

            tryVerify(() => RootData.region.rowCount() === rowsBefore, 5000,
                      "the orphan cave is removed on dismissal")
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
