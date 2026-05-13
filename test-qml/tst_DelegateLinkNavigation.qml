import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Regression test: TapHandlers in DataRightClickMouseMenu must not consume the
// left-click events the underlying LinkText needs to fire its navigation.
MainWindowTest {
    id: rootId

    TestCase {
        name: "DelegateLinkNavigation"
        when: windowShown

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000)
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        function test_clickCaveLinkNavigatesToCavePage() {
            RootData.region.addCave()

            let caveLink = null
            tryVerify(() => {
                caveLink = ObjectFinder.findObjectByChain(mainWindow,
                    "rootId->dataMainPage->caveDelegate0->caveLink")
                return caveLink !== null
            }, 5000, "caveLink should exist")

            mouseClick(caveLink)

            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      3000, "clicking caveLink should navigate to cavePage")
        }
    }
}
