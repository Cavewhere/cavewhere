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
