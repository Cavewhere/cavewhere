import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    CWTestCase {
        name: "KeywordTabLoadProject"
        when: windowShown

        function test_openLayersTab() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            // Ensure we are on the View page
            RootData.pageSelectionModel.gotoPageByName(null, "View");

            let layersTab = findChild(rootId.mainWindow, "layersTabButton");
            verify(layersTab !== null);

            mouseClick(layersTab);

            wait(100000);
        }
    }
}
