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

        function setupExport() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_ScrapInteraction/projectedProfile.cw");

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.05;

            //Put it in profile mode
            let profileButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->GroupBox->profileButton->label")
            mouseClick(profileButton)

            tryVerify(() => { return turnTableInteraction.pitch === 0.0})

            let mapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->mapButton")
            mouseClick(mapButton)

            //Click on the options button
            let options = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->opitonsButton")
            mouseClick(options)


            //Make sure all the margins are displayed correctly
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
            mouseClick(addLayerButton)

            //Make sure we're on the view page
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "viewPage" });

            let selectButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectionToolButton")
            //Make sure the select button is visible, and help is shown correctly
            tryVerify(() => { return selectButton.visible });

            let quoteBox = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->quoteBox")
            verify(quoteBox.visible)

            mouseClick(selectButton)
            let helpBox = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->clickAndDragHelpBox")
            verify(!quoteBox.visible)
            tryVerify(() => {return helpBox.visible})

            //Select an area
            let interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectAreaInteraction")
            mousePress(interaction, 358.566, 251.16)
            mouseMove(interaction, 522.047, 573.117)
            mouseRelease(interaction, 522.047, 573.117)
            verify(!quoteBox.visible)
            tryVerify(() => {return !helpBox.visible})

            //Click done
            mouseClick(selectButton);

            let areaTool = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool");
            tryVerify(() => { return !areaTool.visible })
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "mapPage" });
        }

        function test_exportMap() {
            setupExport();

            //Test that the export works
            verify(false)
        }

        function test_moveMapLayer() {
            setupExport();

            wait(100)

            let captureItem0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0")
            mouseClick(captureItem0_obj1)

            verify(captureItem0_obj1.selected === true)

            console.log("Position on paper:" + captureItem0_obj1.captureItem.positionOnPaper)

            verify(captureItem0_obj1.captureItem.positionOnPaper === Qt.point(1.025, 1.025))

            //Drag
            mouseDrag(captureItem0_obj1, 82.8114, 136.675, 10, 15)

            //Values have been visually verified, that the drag works
            let delta = 0.0001
            fuzzyCompare(captureItem0_obj1.captureItem.positionOnPaper.x, 1.37237, delta)
            fuzzyCompare(captureItem0_obj1.captureItem.positionOnPaper.y, 1.45508, delta)

            //Check that the resize works

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
        }
    }
}
