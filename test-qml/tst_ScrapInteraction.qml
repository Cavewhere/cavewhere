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

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            // wait() needed — newProject() triggers async cleanup (futures, scrap manager,
            // page reloads); tryVerify for viewPage is too narrow to catch all side effects
            wait(1000)
        }

        function setNoteOverlaysCollapsed(collapse) {
            let noteRes = ObjectFinder.findObjectByChain(
                rootId.mainWindow,
                "rootId->tripPage->noteGallery->noteArea->noteResolution")
            if(noteRes) noteRes.collapsed = collapse

            let transformEditor = ObjectFinder.findObjectByChain(
                rootId.mainWindow,
                "rootId->tripPage->noteGallery->noteArea->noteTransformEditor")
            if(transformEditor) transformEditor.collapsed = collapse
        }

        function addScrapOutline() {
            // RootData.futureManagerModel.waitForFinished()
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.qmlTestDatasetPath("tst_ScrapInteraction/projectedProfile.cw"));

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.05;

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            //Select carpet
            let _obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(_obj1);

            // wait() needed — the "" → "SELECT" transition includes PropertyAnimations
            // that reposition the toolbar; clicks on addScrapButton miss during the animation
            wait(500);

            let addScrapButton = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->addScrapButton")
            mouseClick(addScrapButton)

            //Zoom in with wheel mouse
            let imageId = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            let imageCenterX = imageId.width / 2.0;
            let imageCenterY = imageId.height / 2.0;
            for(let i = 0; i < 8; i++) {
                mouseWheel(imageId, imageCenterX, imageCenterY, 0, 120, Qt.NoButton, Qt.NoModifier, 5);
            }

            let noteArea = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea");
            let scrapView = findChild(noteArea, "scrapViewId")
            tryCompare(scrapView, "count", 1);
            verify(scrapView.selectedScrapItem === null)

            mouseMove(imageId, 322, 392);
            mouseClick(imageId, 322, 392)

            //Expand overlays so the checkbox is clickable, then collapse
            //to prevent the overlay from intercepting scrap outline clicks
            setNoteOverlaysCollapsed(false)
            let autoCalculateScrap = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->autoCalculate->checkBox")
            mouseClick(autoCalculateScrap)
            setNoteOverlaysCollapsed(true)

            mouseMove(imageId, 596, 402);
            mouseClick(imageId, 596, 402);
            mouseMove(imageId, 589, 731)
            mouseClick(imageId, 589, 731)
            mouseMove(imageId, 326, 716)
            mouseClick(imageId, 326, 716)

            //Scrap should have 4 points
            tryVerify(() => scrapView.selectedScrapItem !== null, 1000,
                      "scrapView.selectedScrapItem should be non-null after adding points")
            let scrap = scrapView.selectedScrapItem.scrap as Scrap
            verify(scrap !== null)
            verify(scrap.isClosed() === false);
            verify(scrap.numberOfPoints() === 4);

            //Close the scrap by clicking at the first point
            mouseMove(imageId, 322, 392);
            mouseClick(imageId, 322, 392);
            tryVerify(() => { return scrap.isClosed() === true; });
            verify(scrap.numberOfPoints() === 5);

            //Add a point on the right edge of the closed scrap
            mouseMove(imageId, 592, 566);
            mouseClick(imageId, 592, 566);
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

            //Collapse overlays so station clicks aren't intercepted
            setNoteOverlaysCollapsed(true)

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
            tryVerify(() => { return ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->stationa3->coreTextInput") !== null; })
            let a4Station = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->stationa3->coreTextInput")

            //Make sure a4Station scale is correct
            let stationTopLeft = a4Station.mapToItem(noteArea, 0, 0)
            let stationTopRight = a4Station.mapToItem(noteArea, a4Station.width, 0)
            let mappedWidth = stationTopRight.x - stationTopLeft.x
            verify(Math.abs(mappedWidth - a4Station.width) < 0.5)

            // mouseDoubleClickSequence(a4Station)
            a4Station.openEditor() //Double clicking doesn't seem like it works through qml test
            // wait() needed — openEditor() activates the text input asynchronously;
            // activeFocus may not be reliable under --platform offscreen
            wait(50);

            //Change the station to a4
            keyClick("a")
            keyClick(52, 0) //4
            keyClick(16777220, 0) //Return
            verify(scrap.numberOfStations() === 3);

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

            let scrapView = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->scrapViewId")
            tryVerify(() => { return scrapView.visible === false });
        }

        function test_addLeadInteraction() {
            addScrapOutline();

            let _obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->addScrapStation")
            mousePress(_obj1, 28.5313, 28.1914)
            mouseRelease(_obj1, 28.5313, 28.1914)

            //Collapse overlays so station clicks aren't intercepted
            setNoteOverlaysCollapsed(true)

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

            //Add the lead to the note (position shifted to avoid NoteTransformEditor z=2 overlay)
            mouseClick(imageId_obj2, 500, 650)

            //Change the dimensions
            let widthText = null
            tryVerify(() => {
                          widthText = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->leadEditor->widthText")
                          return widthText !== null
                      })
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

            let leadPoint = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_0")
            verify(leadPoint.x >= 0);
            verify(leadPoint.y >= 0);
            verify(leadPoint.x <= render.width)
            verify(leadPoint.y <= render.height)

            //lead is not in the center
            tryVerify(() => { return leadPoint.x !== render.width * 0.5; })
            tryVerify(() => { return leadPoint.y !== render.height * 0.5; })

            mouseClick(leadPoint)

            verify(leadPoint.selected === true)

            //Make sure the popup box is showing the write data
            let leadPointSizeWidth = null
            let leadPointSizeHeight = null
            tryVerify(() => {
                          leadPointSizeWidth = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_0->leadQuoteBox->widthText")
                          leadPointSizeHeight = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_0->leadQuoteBox->heightText")
                          return leadPointSizeWidth !== null && leadPointSizeHeight !== null
                      })

            verify(leadPointSizeWidth.text === "5")
            verify(leadPointSizeHeight.text === "4")

            let description2 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_0->leadQuoteBox->description")
            verify(description2.text === "Sauce")

            //Use the goto notes button to go to the notes
            let notesButton = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->leadPoint2_0->leadQuoteBox->gotoNotes")
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
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.qmlTestDatasetPath("tst_ScrapInteraction/projectedProfile.cw"));

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.05;

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            //Select carpet
            let _obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(_obj1);

            // wait() needed — the "" → "SELECT" transition includes PropertyAnimations
            // that reposition the toolbar; clicks miss during the animation
            wait(500);

            let imageId_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj1, 470.703, 608.133)

            let transformEditor = null
            tryVerify(() => {
                          transformEditor = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor")
                          return transformEditor !== null && transformEditor.scrap !== null
                      })
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

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "viewPage" });

            //Click back
            let back_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->linkBar->back")
            mouseClick(back_obj1, 16.5781, 13.8867)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            //Select the same scrap again
            imageId_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId_obj1, 493.154, 663.103)

            //Make sure the direction is correct
            tryVerify(() => {
                          directionComboBox_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->autoCalculate->directionComboBox")
                          return directionComboBox_obj1 !== null
                      })
            compare(directionComboBox_obj1.currentText, "left → right")

            //Make sure nothing has changed on the scrap level
            transformEditor = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor")
            verify(transformEditor.scrap.type === Scrap.ProjectedProfile)
            verify(transformEditor.scrap.viewMatrix.direction === ProjectedProfileScrapViewMatrix.LeftToRight);
        }
    }
}
