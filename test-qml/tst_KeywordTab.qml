import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

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

            wait(100);

            let addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->addButton0_0")
            mouseClick(addButton0_0_obj1)

            wait(100);

            let removeButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->removeButton0_0")
            mouseClick(removeButton0_0_obj1)

            wait(100)

            //Add it back again
            addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->addButton0_0")
            mouseClick(addButton0_0_obj1)

            wait(100000);
        }
    }
}
