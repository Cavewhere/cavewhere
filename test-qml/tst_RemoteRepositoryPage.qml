import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "RemoteRepositoryPage"
        when: windowShown

        function test_openRemotePage() {
            RootData.pageSelectionModel.currentPageAddress = "Remote"
            // waitForRendering(mainWindow)
            // wait(1000000)
        }
    }
}
