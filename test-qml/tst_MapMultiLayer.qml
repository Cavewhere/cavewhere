import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    SignalSpy {
        id: captureManagerFinished
        signalName: "finishedCapture"
    }

    TestCase {
        name: "MapMultiLayer"
        when: windowShown

        function test_exportMap() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.2;

            let mapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->mapButton")
            mouseClick(mapButton)

            let addLayerButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->addLayerButton")
            mouseClick(addLayerButton)

            wait(50);

            let selectionButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectionToolButton")
            mouseClick(selectionButton)

            let interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectAreaInteraction")
            mouseDrag(interaction, 389.645, 137.965, 340, 349)

            wait(100);

            let done = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectionToolButton->label")
            mouseClick(done)

            wait(50)

            mouseClick(addLayerButton)

            let profileButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->GroupBox->profileButton->label")
            mouseClick(profileButton)

            tryVerify(() => { return turnTableInteraction.pitch === 0.0})

            // let selectionButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectionToolButton")
            mouseClick(selectionButton)

            wait(50);

            //Make sure the selection rectangle is cleared
            let selectionRectangle = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectionRectangle")
            verify(selectionRectangle.visible === true)
            verify(selectionRectangle.width === 0)
            verify(selectionRectangle.height === 0)

            //Select in profile
            interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectAreaInteraction")
            mouseDrag(interaction, 387, 274, 337, 269)
            verify(selectionRectangle.width > 0)
            verify(selectionRectangle.height > 0)

            mouseClick(done);

            wait(100);

            let captureItem0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0")
            mouseClick(captureItem0_obj1, 178.53, 118.409)
            mouseDrag(captureItem0_obj1, 178.53, 118.409, 0, -30)

            let captureItem1_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem1")
            mouseClick(captureItem1_obj1, 177.889, 282.772)

            verify(captureItem0_obj1.selected === false);
            verify(captureItem1_obj1.selected === true);

            mouseDrag(captureItem1_obj1, 216.432, 153.995, 0, 55)

            //Export the image
            let mapPage = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage")
            let screenCaptureManager = findChild(mapPage, "screenCaptureManager")
            captureManagerFinished.target = screenCaptureManager
            screenCaptureManager.filename = Qt.resolvedUrl("test.svg")
            screenCaptureManager.fileType = CaptureManager.SVG
            screenCaptureManager.capture()
            captureManagerFinished.wait(5000)

            wait(100);
        }
    }
}
