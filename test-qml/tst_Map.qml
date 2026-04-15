import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    CWTestCase {
        name: "Map"
        when: windowShown

        function cleanup() {
            let mapPage = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->mapPage"); //"->screenCaptureManager")
            let catpureManager = findChild(mapPage, "screenCaptureManager")
            while(catpureManager.numberOfCaptures > 0) {
                let index = catpureManager.index(0);
                let capture = catpureManager.data(index, CaptureManager.LayerObjectRole);
                catpureManager.removeCaptureViewport(capture)
            }

            //Return the paper back to it's original
            let paperComboBox_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->GroupBox->paperComboBox")
            let customPaperIndex = paperComboBox_obj1.find("Letter")
            paperComboBox_obj1.currentIndex = customPaperIndex
        }

        function setupExport() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.qmlTestDatasetPath("tst_ScrapInteraction/projectedProfile.cw"));

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.05;

            let profileButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->cameraOptions->GroupBox->profileButton")
            mouseClick(profileButton)

            tryVerify(() => { return turnTableInteraction.pitch === 0.0})

            let mapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->mapButton")
            mouseClick(mapButton)


            //Make sure we're on the map page
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "mapPage" });

            //Reset margins to known state and verify
            let paperMargin = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->paperMargin")
            paperMargin.setDefaultLeft(1.0)
            paperMargin.setDefaultRight(1.0)
            paperMargin.setDefaultTop(1.0)
            paperMargin.setDefaultBottom(1.0)

            let topMargin = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->paperMargin->PaperMarginGroupBox->topMarginSpinBox->spinBox")
            let bottomMargin = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->paperMargin->PaperMarginGroupBox->bottomMarginSpinBox->spinBox")
            let rightMargin = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->paperMargin->PaperMarginGroupBox->rightMarginSpinBox->spinBox")
            let leftMargin = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->paperMargin->PaperMarginGroupBox->leftMarginSpinBox->spinBox")
            let allMargin = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->paperMargin->PaperMarginGroupBox->allMarginSpinBox->spinBox")

            verify(topMargin.realValue === 1.0)
            verify(topMargin.displayText === "1.00")
            verify(bottomMargin.realValue === 1.0)
            verify(bottomMargin.displayText === "1.00")
            verify(leftMargin.realValue === 1.0)
            verify(leftMargin.displayText === "1.00")
            verify(rightMargin.realValue === 1.0)
            verify(rightMargin.displayText === "1.00")
            verify(allMargin.realValue === 1.0)
            verify(allMargin.displayText === "1.00")

            //Click on the add layer button
            let addLayerButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->addLayerButton")
            waitForRendering(addLayerButton)
            mouseClick(addLayerButton)

            //Make sure we're on the view page
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "viewPage" });

            let selectButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectionToolButton")
            //Make sure the select button is visible, and help is shown correctly
            tryVerify(() => { return selectButton.visible });

            let quoteBox = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->quoteBox")
            tryVerify(() => { return quoteBox.visible });

            mouseClick(selectButton)

            verify(selectButton.visible === true);
            verify(selectButton.enabled === false);

            let helpBox = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->clickAndDragHelpBox")
            tryVerify(() => {return !quoteBox.visible })
            tryVerify(() => {return helpBox.visible})

            //Select an area
            let interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectAreaInteraction")
            mouseDrag(interaction, 358, 251, 164, 322)
            verify(!quoteBox.visible)
            tryVerify(() => {return !helpBox.visible})

            //Click done
            tryVerify(() => { return selectButton.visible === true });
            tryVerify(() => { return selectButton.enabled === true });
            tryVerify(() => { return selectButton.text === " Done" })
            mouseClick(selectButton);

            // wait() needed — the capture viewport geometry is computed asynchronously
            // after the Done button is clicked; without it the capture item has wrong dimensions
            wait(100);

            let areaTool = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool");
            tryVerify(() => { return !areaTool.visible })
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "mapPage" });
        }

        function test_exportMap() {
            setupExport();

            //Test that the export works
            // verify(false)

            // wait(100000)
        }

        function moveMapLayer() {
            let captureItem0_obj1 = null
            tryVerify(() => {
                          captureItem0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0")
                          return captureItem0_obj1 !== null
                      })
            mouseClick(captureItem0_obj1)

            tryVerify(() => { return captureItem0_obj1.selected === true })
            verify(captureItem0_obj1.captureItem.positionOnPaper === Qt.point(1.025, 1.025))

            //Drag
            mouseDrag(captureItem0_obj1, 82.8114, 136.675, 10, 15)

            //Values have been visually verified, that the drag works
            let delta = 0.02
            //Wait for drag position to be computed before tryFuzzyCompare captures value
            tryVerify(() => { return Math.abs(captureItem0_obj1.captureItem.positionOnPaper.x - 1.19041) <= delta })
            tryFuzzyCompare(captureItem0_obj1.captureItem.positionOnPaper.x, 1.19041, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.positionOnPaper.y, 1.27312, delta)

            return captureItem0_obj1
        }

        function test_rotateMapLayer() {
            setupExport()

            //Select the map layer
            let captureItem0_obj1 = moveMapLayer()

            //Go into rotation mode
            mouseClick(captureItem0_obj1)

            //Do rotation on top left corner
            let rotationHandle_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0->rotationHandle")
            // wait() between mouse steps is needed — the rotation interaction
            // processes events through timers to distinguish click from drag
            mousePress(rotationHandle_obj1, 24.8349, 8.20597)
            wait(50)
            mouseMove(rotationHandle_obj1, 24.8349, 8.20597+10)
            wait(50)
            mouseMove(rotationHandle_obj1, 24.8349+10, 8.20597+10)
            wait(50)
            mouseMove(rotationHandle_obj1, 24.8349+10, 8.20597+30)
            wait(50)
            mouseRelease(rotationHandle_obj1, 24.8349+10, 8.20597+30)

            let delta = 0.02
            //Wait for rotation to be computed before tryFuzzyCompare captures value
            tryVerify(() => { return Math.abs(captureItem0_obj1.captureItem.boundingBox.x - (-0.570247)) <= delta })
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.x, -0.570247, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.y, 0.726273, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.width, 8.07971, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.height, 10.0437, delta);

            //Move sure move the layer works in rotation
            mouseDrag(captureItem0_obj1, 82.8114, 136.675, 50, 40)

            //Values have been visually verified, that the drag works
            tryVerify(() => { return Math.abs(captureItem0_obj1.captureItem.positionOnPaper.x - 2.86109) <= delta })
            tryFuzzyCompare(captureItem0_obj1.captureItem.positionOnPaper.x, 2.86109, delta)
            tryFuzzyCompare(captureItem0_obj1.captureItem.positionOnPaper.y, 2.57988, delta)
        }

        function test_moveResizeMapLayer() {
            setupExport();

            let captureItem0_obj1 = moveMapLayer()

            // wait(10000);

            let topLeftHandle_obj1 = null
            tryVerify(() => {
                          topLeftHandle_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0->topLeftHandle")
                          return topLeftHandle_obj1 !== null
                      })
            mouseDrag(topLeftHandle_obj1, 5, 5, 5, 10, Qt.LeftButton, Qt.NoModifier, 50)


            //Values have been visually verified, that the drag works
            let delta = 0.02
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.x, 1.27312, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.y, 1.43551, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.width, 4.47568, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.height, 8.78761, delta);


            let topRightHandle_obj1 = null
            tryVerify(() => {
                          topRightHandle_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0->topRightHandle")
                          return topRightHandle_obj1 !== null
                      })
            mouseDrag(topRightHandle_obj1, 5, 5, 10, 7, Qt.LeftButton, Qt.NoModifier, 50)

            //Values have been visually verified, that the drag works
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.x, 1.27312, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.y, 1.11073, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.width, 4.64109, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.height, 9.11239, delta);

            let bottomRightHandle_obj1 = null
            tryVerify(() => {
                          bottomRightHandle_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0->bottomRightHandle")
                          return bottomRightHandle_obj1 !== null
                      })
            mouseDrag(bottomRightHandle_obj1, 5, 5, 8, 10, Qt.LeftButton, Qt.NoModifier, 50)

            //Values have been visually verified, that the drag works
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.x, 1.27312, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.y, 1.11073, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.width, 4.77342, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.height, 9.37221, delta);

            let bottomLeftHandle_obj1 = null
            tryVerify(() => {
                          bottomLeftHandle_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0->bottomLeftHandle")
                          return bottomLeftHandle_obj1 !== null
                      })
            mouseDrag(bottomLeftHandle_obj1, 5, 5, 4, 15, Qt.LeftButton, Qt.NoModifier, 50)

            //Values have been visually verified, that the drag works
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.x, 1.14675, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.y, 1.11073, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.width, 4.89979, delta);
            tryFuzzyCompare(captureItem0_obj1.captureItem.boundingBox.height, 9.62033, delta);
        }

        function test_paperSize() {
            setupExport()

            //Change to orientation to landscape
            let landscapeButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->GroupBox->orientaitonSwitch")
            mouseClick(landscapeButton)

            let paperWidth = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->GroupBox->paperWidthInput")
            let paperHeight = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->GroupBox->paperHeightInput")

            verify(paperWidth.text === "11")
            verify(paperHeight.text === "8.5")
            verify(landscapeButton.visible === true);

            //Change the paper size
            let paperComboBox_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->GroupBox->paperComboBox")
            let customPaperIndex = paperComboBox_obj1.find("Custom Size")
            paperComboBox_obj1.currentIndex = customPaperIndex
            verify(paperComboBox_obj1.currentText === "Custom Size")

            //Make sure orientation is disabled
            verify(landscapeButton.visible === false);

            //Change the paper height to 14
            mouseClick(paperHeight)
            keyClick(49, 0) //1
            keyClick(53, 0) //5
            keyClick(16777220, 0) //Return

            //Make the paper width and height is correct
            verify(paperHeight.text === "15")

            mouseClick(paperWidth)
            keyClick(49, 0) //1
            keyClick(48, 0) //0
            keyClick(16777220, 0) //Return

            //Make the paper width and height is correct
            verify(paperWidth.text === "10")

            //Switch to letter
            let legalPaperIndex = paperComboBox_obj1.find("Legal")
            paperComboBox_obj1.currentIndex = legalPaperIndex
            verify(paperComboBox_obj1.currentText === "Legal")

            //Make sure orientation is disabled
            verify(landscapeButton.visible === true);

            //Verify that we're still in land scape
            verify(paperWidth.text === "14")
            verify(paperHeight.text === "8.5")

            // wait() needed — paper layout must settle after Legal switch
            // before the landscape toggle click registers correctly
            wait(100)

            //Click on landscapeButton again to put in portrate
            mouseClick(landscapeButton)

            //Make sure we flipped
            verify(paperWidth.text === "8.5")
            verify(paperHeight.text === "14")

            //Switch back to custom, make sure the custom numbers are remembered
            customPaperIndex = paperComboBox_obj1.find("Custom Size")
            paperComboBox_obj1.currentIndex = customPaperIndex
            verify(paperComboBox_obj1.currentText === "Custom Size")
            verify(paperHeight.text === "15")
            verify(paperWidth.text === "10")

            customPaperIndex = paperComboBox_obj1.find("Letter")
            paperComboBox_obj1.currentIndex = customPaperIndex
            verify(paperComboBox_obj1.currentText === "Letter")
        }

        //The captures should resize correctly when the paper resizes
        function test_captureResizeOnpaperResize() {
            setupExport()

            //Change the scale of the item
            let inCaveLength = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->GroupBox->inCaveLengthInput->coreTextInput")
            mouseClick(inCaveLength)

            //Change the scale to 1:20
            keyClick(50, 0) //2
            keyClick(48, 0) //0
            keyClick(16777220, 0) //Return

            let paperComboBox_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->GroupBox->paperComboBox")
            let customSizeIndex = paperComboBox_obj1.find("Custom Size");

            //make sure the index is valid
            verify(customSizeIndex >= 0);

            //Switch to custom size index
            paperComboBox_obj1.currentIndex = customSizeIndex;

            let paperWidth = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->GroupBox->paperWidthInput")
            mouseClick(paperWidth)

            keyClick(49, 0) //1
            keyClick(56, 0) //8
            keyClick(16777220, 0) //Return

            let paperHeight = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->GroupBox->paperHeightInput")
            mouseClick(paperHeight)

            keyClick(51, 0) //3
            keyClick(53, 0) //5
            keyClick(16777220, 0) //Return

            //Make sure the capture is the correct size
            let captureItem0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0")

            //Wait for layout to settle before tryFuzzyCompare captures value
            tryVerify(() => { return Math.abs(captureItem0_obj1.width - 306) <= 5.0 })
            tryFuzzyCompare(captureItem0_obj1.width, 306, 5.0);
            tryFuzzyCompare(captureItem0_obj1.height, 602, 5.0);
            tryFuzzyCompare(captureItem0_obj1.x, 19.5, 5.0);
            tryFuzzyCompare(captureItem0_obj1.y, 19.5, 5.0);

        }
    }
}
