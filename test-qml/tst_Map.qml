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

            wait(100);

            //Click on help
            let helpMemory = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->memoryHelpArea")
            mouseClick(helpMemory)



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
