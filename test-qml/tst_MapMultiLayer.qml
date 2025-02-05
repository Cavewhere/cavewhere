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

            let profileButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->cameraOptions->GroupBox->profileButton")
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

            //Select in profile north
            interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectAreaInteraction")
            mouseDrag(interaction, 387, 274, 337, 269)
            verify(selectionRectangle.width > 0)
            verify(selectionRectangle.height > 0)

            mouseClick(done);

            wait(100);

            mouseClick(addLayerButton)

            //Look east
            let eastButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->cameraOptions->GroupBox->eastButton")
            mouseClick(eastButton)
            tryVerify(() => { return turnTableInteraction.azimuth === 90.0})

            //Select in profile east
            mouseClick(selectionButton)
            interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectAreaInteraction")
            mouseDrag(interaction, 275, 305, 279, 267)
            mouseClick(done);

            wait(100);

            //Make sure cycle selection works correctly
            let captureItem0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0")
            let captureItem1_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem1")
            let captureItem2_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem2")

            let selectedCapture = function() {
                if(captureItem0_obj1.selected) {
                    return captureItem0_obj1
                } else if(captureItem1_obj1.selected) {
                    return captureItem1_obj1
                } else if(captureItem2_obj1.selected) {
                    return captureItem2_obj1
                }
                return null
            }

            mouseClick(captureItem2_obj1, 281.029, 181.485)
            wait(50);
            let firstSelected = selectedCapture();

            mouseClick(captureItem2_obj1, 281.029, 181.485)
            wait(50);
            let secondSelected = selectedCapture();

            mouseClick(captureItem2_obj1, 281.029, 181.485)
            wait(50);
            let thirdSelected = selectedCapture();

            verify(firstSelected !== secondSelected);
            verify(firstSelected !== thirdSelected);
            verify(secondSelected !== thirdSelected);

            mouseClick(captureItem2_obj1, 281.029, 181.485)
            wait(50);
            verify(firstSelected === selectedCapture())

            mouseClick(captureItem1_obj1, 177.889, 282.772)
            wait(50);

            verify(captureItem0_obj1.selected === false);
            verify(captureItem2_obj1.selected === false);
            verify(captureItem1_obj1.selected === true);

            //Spot check that the elements are visually correct
            verify(findChild(captureItem0_obj1, "topLeftHandle").visible == false);
            verify(findChild(captureItem2_obj1, "topLeftHandle").visible == false);
            verify(findChild(captureItem1_obj1, "topLeftHandle").visible == true); //Should be true because it's selected

            mouseDrag(captureItem1_obj1, 216.432, 153.995, 0, 50)

            let layerListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->GroupBox->layerListView")
            verify(layerListView.currentIndex === 1) //The second layer should be selected

            //Select a layer through the layer list
            let layerDelegate0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->GroupBox->layerListView->layerDelegate0")
            mouseClick(layerDelegate0)
            verify(layerListView.currentIndex === 0)
            verify(findChild(captureItem0_obj1, "topLeftHandle").visible == true);
            verify(findChild(captureItem2_obj1, "topLeftHandle").visible == false);
            verify(findChild(captureItem1_obj1, "topLeftHandle").visible == false);

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
