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
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->SplitView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->SplitView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.2;

            let mapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->mapButton")
            mouseClick(mapButton)

            // wait() needed — mapPage layout must complete before addLayerButton is interactive
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "mapPage" });
            wait(100)

            let addLayerButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->addLayerButton")
            mouseClick(addLayerButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "viewPage" });

            let selectionButton = null
            tryVerify(() => {
                          selectionButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectionToolButton")
                          return selectionButton !== null && selectionButton.visible
                      })
            mouseClick(selectionButton)

            let interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectAreaInteraction")
            mouseDrag(interaction, 389.645, 137.965, 340, 349)

            tryVerify(() => { return selectionButton.enabled === true })
            let done = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectionToolButton->label")
            mouseClick(done)

            // wait() needed — capture viewport geometry is computed asynchronously
            // after Done click; without it the capture item has wrong dimensions
            wait(50)

            mouseClick(addLayerButton)

            let profileButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->cameraOptions->GroupBox->profileButton")
            mouseClick(profileButton)
            tryVerify(() => { return turnTableInteraction.pitch === 0.0})

            // let selectionButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectionToolButton")
            mouseClick(selectionButton)

            //Make sure the selection rectangle is cleared
            let selectionRectangle = null
            tryVerify(() => {
                          selectionRectangle = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectionRectangle")
                          return selectionRectangle !== null && selectionRectangle.visible === true
                      })
            verify(selectionRectangle.width === 0)
            verify(selectionRectangle.height === 0)

            //Select in profile north
            interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectAreaInteraction")
            mouseDrag(interaction, 387, 274, 337, 269)
            verify(selectionRectangle.width > 0)
            verify(selectionRectangle.height > 0)

            mouseClick(done);

            // wait() needed — capture viewport geometry is computed asynchronously
            wait(100);

            mouseClick(addLayerButton)

            //Look east
            let eastButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->cameraOptions->GroupBox->eastButton")
            mouseClick(eastButton)
            tryVerify(() => { return turnTableInteraction.azimuth === 90.0})

            //Select in profile east
            mouseClick(selectionButton)
            interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectAreaInteraction")
            mouseDrag(interaction, 275, 305, 279, 267)
            mouseClick(done);

            //Make sure cycle selection works correctly
            let captureItem0_obj1 = null
            tryVerify(() => {
                          captureItem0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0")
                          return captureItem0_obj1 !== null
                      })
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
            tryVerify(() => { return selectedCapture() !== null })
            let firstSelected = selectedCapture();

            mouseClick(captureItem2_obj1, 281.029, 181.485)
            tryVerify(() => { return selectedCapture() !== null && selectedCapture() !== firstSelected })
            let secondSelected = selectedCapture();

            mouseClick(captureItem2_obj1, 281.029, 181.485)
            tryVerify(() => { return selectedCapture() !== null && selectedCapture() !== secondSelected })
            let thirdSelected = selectedCapture();

            verify(firstSelected !== secondSelected);
            verify(firstSelected !== thirdSelected);
            verify(secondSelected !== thirdSelected);

            mouseClick(captureItem2_obj1, 281.029, 181.485)
            tryVerify(() => { return firstSelected === selectedCapture() })

            mouseClick(captureItem1_obj1, 177.889, 282.772)
            tryVerify(() => { return captureItem1_obj1.selected === true })

            verify(captureItem0_obj1.selected === false);
            verify(captureItem2_obj1.selected === false);

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
            function exportImage(filename, fileType) {
                let localFileUrl = Qt.resolvedUrl(filename)
                TestHelper.removeFile(localFileUrl);
                verify(!TestHelper.fileExistslocalFileUrl);

                let mapPage = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage")
                let screenCaptureManager = findChild(mapPage, "screenCaptureManager")
                captureManagerFinished.target = screenCaptureManager
                screenCaptureManager.filename = localFileUrl
                screenCaptureManager.fileType = fileType
                screenCaptureManager.capture()
                captureManagerFinished.wait(10000)

                verify(TestHelper.fileExists(localFileUrl));
                verify(TestHelper.fileSize(localFileUrl) > 0);
            }

            exportImage("test.svg", CaptureManager.SVG);
            exportImage("test.pdf", CaptureManager.PDF);
            exportImage("test.png", CaptureManager.PNG);
        }
    }
}
