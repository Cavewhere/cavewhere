import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "NoteScrapInteraction"
        when: windowShown

        function addScrapOutline() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_ScrapInteraction/projectedProfile.cw");

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.05;

            wait(100);

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            //Select carpet
            let _obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(_obj1);

            wait(500);

            let addScrapButton = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->addScrapButton")
            mouseClick(addScrapButton)

            wait(200);

            let imageId = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseMove(imageId, 322, 392);
            mouseClick(imageId, 322, 392)
            wait(50);

            let autoCalculateScrap = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->autoCalculate->checkBox")
            mouseClick(autoCalculateScrap)

            mouseMove(imageId, 596, 402);
            mouseClick(imageId, 596, 402);
            wait(50)
            mouseMove(imageId, 589, 731)
            mouseClick(imageId, 589, 731)
            wait(50)
            mouseMove(imageId, 326, 716)
            mouseClick(imageId, 326, 716)

            //Scrap should have 4 points:
            let noteArea = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea");
            let scrapView = findChild(noteArea, "scrapViewId")
            let scrap = scrapView.selectedScrapItem.scrap as Scrap
            verify(scrap !== null)
            verify(scrap.isClosed() === false);
            verify(scrap.numberOfPoints() === 4);

            //Close the scrap
            mouseMove(imageId, 322, 392);
            mouseClick(imageId, 322, 392)
            verify(scrap.isClosed() === true);
            verify(scrap.numberOfPoints() === 5);

            //Add a point in the line
            mouseMove(imageId, 451, 396);
            mouseClick(imageId, 451, 396);
            verify(scrap.isClosed() === true);
            verify(scrap.numberOfPoints() === 6);

            // let pointItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId->scrapPointItem1")
            // mousePress(pointItem, 6.77947, 7.29086)
            // mouseMove(pointItem, 8, 8);
            // mouseRelease(pointItem, 8, 8)

            // // mouseClick(imageId, 3.26563, -2.60156)
            // // mouseClick(imageId, 145.691, -13.3125)
            // // mouseClick(imageId, 154.125, 124.215)
            // // mouseClick(imageId, -2.42578, 127.035)
        }

        function test_addScrapInteraction() {
            addScrapOutline()
        }

        function zoomIntoRenderingView() {
            //Switch to rendering view
            let viewButton = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->mainSideBar->viewButton")
            mouseClick(viewButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "viewPage" });
            wait(100);
            // let renderingView = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer")
            // let renderingViewCenterX = renderingView.width / 2.0;
            // let renderingViewCenterY = renderingView.height / 2.0;

            // for(let i = 0; i < 200; i++) {
            //     mouseWheel(renderingView, renderingViewCenterX, renderingViewCenterY, 100, 100, Qt.LeftButton, Qt.NoModifier, 5)
            // }
        }

        function test_addStationInteraction() {
            addScrapOutline()

            let addScrapStationButton = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->addScrapStation")
            mouseClick(addScrapStationButton)

            let imageId = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId, 391.749, 650.222)
            keyClick("a")
            keyClick(51, 0) //3
            keyClick(16777220, 0) //Return

            mouseClick(imageId, 448.07, 440.733)

            //Make sure that this is a valid scrap
            let noteArea = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea");
            let scrapView = findChild(noteArea, "scrapViewId")
            let scrap = scrapView.selectedScrapItem.scrap as Scrap
            verify(scrap !== null)
            verify(scrap.numberOfStations() === 2);

            //Add a new station
            mouseClick(imageId, 509, 630);

            //Double click on the stations text
            tryVerify(() => { return ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->stationA3->coreTextInput") !== null; })
            let a4Station = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->stationA3->coreTextInput")

            // mouseDoubleClickSequence(a4Station)
            a4Station.openEditor() //Double clicking doesn't seem like it works through qml test

            wait(50);

            //Change the station to a4
            keyClick("a")
            keyClick(52, 0) //4
            keyClick(16777220, 0) //Return
            verify(scrap.numberOfStations() === 3);

            wait(50);

            //Switch to rendering view
            zoomIntoRenderingView()
        }

        function test_deselection() {
            addScrapOutline();


            let backButton = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->backButton")
            mouseClick(backButton)

            let noteGallery = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery")
            tryVerify(() => { return noteGallery.state === "" }) ;

            let noteItem = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea")
            tryVerify(() => { return noteItem.scrapsVisible === false });

            let scrapView = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId->scrapViewId")
            tryVerify(() => { return scrapView.visible === false });

        }

        function test_addLeadInteraction() {
            addScrapOutline();

            let _obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->addScrapStation")
            mousePress(_obj1, 28.5313, 28.1914)
            mouseRelease(_obj1, 28.5313, 28.1914)

            let imageId_obj2 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj2, 391.974, 651.705)

            keyClick("a")
            keyClick(51, 0) //3
            keyClick(16777220, 0) //Return

            //Add a second station
            mouseClick(imageId_obj2, 509.277, 631.702)

            let a4Station = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->stationA2->coreTextInput")
            a4Station.openEditor() //Double clicking doesn't seem like it works through qml test

            keyClick("a")
            keyClick(52, 0) //4
            keyClick(16777220, 0) //Return

            //Click on the leads button
            let addLeads = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->addLeads")
            mouseClick(addLeads)

            //Add the lead to the note
            mouseClick(imageId_obj2, 523.515, 506.278)

            //Change the dimensions
            let widthText = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->widthText")
            mouseClick(widthText, 1.78125, 9.35938)

            //5 for the width
            keyClick(53, 0) //5

            let heightText = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->heightText")
            mouseClick(heightText, 3, 10)

            //4 for the height
            keyClick(52, 0) //4

            let description = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->description")
            mouseClick(description)

            keyClick("S")
            keyClick("a")
            keyClick("u")
            keyClick("c")
            keyClick("e")

            //Make sure that this is a valid scrap and lead data has been correctly update
            let noteArea = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea");
            let scrapView = findChild(noteArea, "scrapViewId")
            let scrap = scrapView.selectedScrapItem.scrap as Scrap
            verify(scrap !== null)
            verify(scrap.numberOfStations() === 2);
            verify(scrap.numberOfLeads() == 1);
            verify(scrap.leadData(Scrap.LeadSize, 0).width === 5.0)
            verify(scrap.leadData(Scrap.LeadSize, 0).height === 4.0)
            verify(scrap.leadData(Scrap.LeadDesciption, 0) === "Sauce")

            //Make sure the lead is being displayed correctly in the 3d view
            //Switch to rendering view
            zoomIntoRenderingView();

            let render = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer")

            let leadPoint = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint1_0")
            verify(leadPoint.x >= 0);
            verify(leadPoint.y >= 0);
            verify(leadPoint.x <= render.width)
            verify(leadPoint.y <= render.height)

            //lead is not in the center
            tryVerify(() => { return leadPoint.x !== render.width * 0.5; })
            tryVerify(() => { return leadPoint.y !== render.height * 0.5; })

            mouseClick(leadPoint)

            verify(leadPoint.selected === true)

            wait(200);

            //Make sure the popup box is showing the write data
            let leadPointSizeWidth = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint1_0->leadQuoteBox->widthText")
            let leadPointSizeHeight = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint1_0->leadQuoteBox->heightText")

            verify(leadPointSizeWidth.text === "5")
            verify(leadPointSizeHeight.text === "4")

            let description2 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint1_0->leadQuoteBox->description")
            verify(description2.text === "Sauce")

            //Use the goto notes button to go to the notes
            let notesButton = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint1_0->leadQuoteBox->gotoNotes")
            mouseClick(notesButton)

            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "tripPage" });

            //Select the carpet button
            let carpet = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(carpet);

            //Select the select button
            let select = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->selectButton")
            mouseClick(select)

            //Select the lead
            let imageId_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj3, 555.655, 441.759)
            let noteLead = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteLead0")
            mouseClick(noteLead)
            verify(noteLead.selected === true)

            //Delete the select lead
            keyClick(16777219, 0) //Backspace

            //Switch back to the normal view
            let viewButton = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->mainSideBar->viewButton")
            mouseClick(viewButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "viewPage" });
        }

        /**
          Make sure the projected profile direction stays, and doesn't change once
          set
          */
        function test_projectedProfileDirection() {


            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_ScrapInteraction/projectedProfile.cw");

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.05;

            wait(100);

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            //Select carpet
            let _obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(_obj1);

            wait(500);

            let imageId_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj1, 470.703, 608.133)

            let transformEditor = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor")
            verify(transformEditor.scrap.type === Scrap.ProjectedProfile)
            verify(transformEditor.scrap.viewMatrix.direction === ProjectedProfileScrapViewMatrix.LookingAt);

            //Change the type to LeftToRight
            transformEditor.scrap.viewMatrix.direction = ProjectedProfileScrapViewMatrix.LeftToRight
            verify(transformEditor.scrap.viewMatrix.direction === ProjectedProfileScrapViewMatrix.LeftToRight);

            //Make sure the combobox updates
            let directionComboBox_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->autoCalculate->directionComboBox")
            compare(directionComboBox_obj1.currentText, "left → right")

            //Click on view
            let viewButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->viewButton")
            mouseClick(viewButton_obj1)

            wait(500)

            //Click back
            let back_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->linkBar->back")
            mouseClick(back_obj1, 16.5781, 13.8867)

            wait(500)

            //Select the same scrap again
            imageId_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj1, 493.154, 663.103)

            wait(100)

            //Make sure the direction is correct
            directionComboBox_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->autoCalculate->directionComboBox")
            compare(directionComboBox_obj1.currentText, "left → right")

            //Make sure nothing has changed on the scrap level
            transformEditor = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor")
            verify(transformEditor.scrap.type === Scrap.ProjectedProfile)
            verify(transformEditor.scrap.viewMatrix.direction === ProjectedProfileScrapViewMatrix.LeftToRight);
        }
    }
}
