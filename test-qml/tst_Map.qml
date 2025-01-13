import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "Map"
        when: windowShown

        function test_exportMap() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_ScrapInteraction/projectedProfile.cw");

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.05;

            let mapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->mapButton")
            mouseClick(mapButton)

            //Click on the options button
            let options = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->opitonsButton")
            mouseClick(options)





            // let label_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->GroupBox->profileButton->label")
            // mouseClick(label_obj1, 14.5273, 14.9609)

            // tryVerify(() => { return turnTableInteraction.pitch === 0.0})

            // let exportTab = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->exportTab")
            // mouseClick(exportTab)

            // let optionsButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->exportViewTab->opitonsButton")
            // mouseClick(optionsButton)





            wait(1000000)
        }
    }
}
