import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "DataMainPageExternal"
        when: windowShown

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        function gotoDataMainPage() {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "should land on dataMainPage")
            return RootData.pageView.currentPageItem
        }

        function openCavernOutputMenuItem(dataPage) {
            const menuButton = findChild(dataPage, "regionContextMenu")
            verify(menuButton !== null, "regionContextMenu must exist on DataMainPage")
            mouseClick(menuButton)

            const cavernItem = findChild(mainWindow, "cavernOutputMenuItem")
            verify(cavernItem !== null,
                   "Cavern Output menu item must exist in the regionContextMenu")
            return cavernItem
        }

        // Opens the Add split button's menu and returns it.
        function openAddMenu(dataPage) {
            // The action bar is placed by the page's layout; clicks miss
            // until it settles.
            waitForRendering(rootId)
            const addBar = findChild(dataPage, "addCave")
            verify(addBar !== null, "the Add split button must exist")
            const menuButton = findChild(addBar, "menuButton")
            verify(menuButton !== null && menuButton.visible, "the split button shows its caret")
            mouseClick(menuButton)

            let menu = null
            tryVerify(() => {
                          menu = findChild(dataPage, "addCaveMenu")
                          return menu !== null && menu.opened
                      }, 5000, "the caret opens the Add menu")
            return menu
        }

        // The Add button adds a cave itself, so its menu holds Add Folder and
        // Attach survey file…, and nothing else.
        function test_addMenuHoldsFolderAndSurveyFileOnly() {
            const dataPage = gotoDataMainPage()
            const menu = openAddMenu(dataPage)

            const names = []
            for (let i = 0; i < menu.count; i++) {
                names.push(menu.itemAt(i).objectName)
            }
            compare(names, ["addFolderMenuItem", "attachSurveyFileMenuItem"])
            compare(menu.itemAt(0).text, "Add Folder")
            compare(menu.itemAt(1).text, "Attach survey file…")
            verify(String(menu.itemAt(0).icon.source).indexOf("folder.svg") >= 0,
                   "Add Folder carries the folder glyph")
            menu.close()
        }

        // Attach survey file… opens the dialog with both choices: Attach keeps
        // the file as the source, Import copies it into a native trip.
        function test_addFromSurveyFileOffersAttachAndImport() {
            const dataPage = gotoDataMainPage()
            const menu = openAddMenu(dataPage)
            mouseClick(findChild(menu, "attachSurveyFileMenuItem"))

            let dialog = null
            tryVerify(() => {
                          dialog = findChild(dataPage, "attachSurveyFileDialog")
                          return dialog !== null
                      }, 5000, "the dialog lives on the Data page")
            const pathField = findChild(dialog, "sourcePathField")
            tryVerify(() => pathField !== null && pathField.visible, 5000, "the dialog opens")
            verify(findChild(dialog, "attachButton") !== null, "the dialog offers Attach")
            verify(findChild(dialog, "importButton") !== null, "the dialog offers Import a copy")
            compare(RootData.region.caveCount, 0, "opening the dialog creates nothing")

            mouseClick(findChild(dialog, "cancelButton"))
            tryVerify(() => !pathField.visible, 5000, "Cancel closes the dialog")
            compare(RootData.region.caveCount, 0)
        }

        // The primary half of the split button adds a cave and opens its page.
        function test_addButtonAddsACave() {
            const dataPage = gotoDataMainPage()
            waitForRendering(rootId)
            const addButton = findChild(findChild(dataPage, "addCave"), "addButton")
            verify(addButton !== null, "the Add button must exist")
            compare(addButton.text, "Add Cave")
            mouseClick(addButton)

            tryCompare(RootData.region, "caveCount", 1, 5000)
            compare(RootData.region.cave(0).kind, SurveyNodeKind.Cave, "the button adds a Cave")
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage"
                            && RootData.pageView.currentPageItem.currentNode === RootData.region.cave(0),
                      5000, "the new cave's page opens")
        }

        function test_menuItemStaysPlainAndNavigates() {
            // Decided 2026-07-19: no live stats in the context menu — the
            // item reads plain "Cavern Output" (stats live on the Cavern
            // page's status label instead).
            const dataPage = gotoDataMainPage()
            const cavernItem = openCavernOutputMenuItem(dataPage)

            compare(cavernItem.text, qsTr("Cavern Output"))

            mouseClick(cavernItem)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavernOutputPage",
                      5000, "clicking the item must land on the Cavern page")
        }

        function test_menuItemStaysPlainAfterSolve() {
            TestHelper.loadProjectFromFile(
                RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"))
            RootData.futureManagerModel.waitForFinished()

            tryVerify(() => RootData.linePlotManager.lastSolveDuration >= 0,
                      10000, "a solve should have stamped the duration")

            const dataPage = gotoDataMainPage()
            const cavernItem = openCavernOutputMenuItem(dataPage)
            compare(cavernItem.text, qsTr("Cavern Output"))
        }
    }
}
