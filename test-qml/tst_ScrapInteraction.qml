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
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

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

        function test_addStationInteraction() {
            addScrapOutline()

            let addScrapStationButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addScrapStation")
            mouseClick(addScrapStationButton)

            let imageId = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId, 391.749, 650.222)
            keyClick("a")
            keyClick(51, 0) //3
            keyClick(16777220, 0) //Return

            mouseClick(imageId, 448.07, 440.733)

            wait(100);

            //Make sure that this is a valid scrap
            let noteArea = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea");
            let scrapView = findChild(noteArea, "scrapViewId")
            let scrap = scrapView.selectedScrapItem.scrap as Scrap
            verify(scrap !== null)
            verify(scrap.numberOfStations() === 2);


            //Switch to rendering view
            let viewButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->viewButton")
            mouseClick(viewButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "viewPage" });

            let renderingView = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer")
            let renderingViewCenterX = renderingView.width / 2.0;
            let renderingViewCenterY = renderingView.height / 2.0;

            for(let i = 0; i < 200; i++) {
                mouseWheel(renderingView, renderingViewCenterX, renderingViewCenterY, 100, 100, Qt.LeftButton, Qt.NoModifier, 5)
            }

            wait(100);

            //Make sure the scrap is being rendered, scrap is white
            let renderImage = grabImage(renderingView)
            compare(renderImage.pixel(617,257), Qt.rgba(1, 1, 1, 255));

            //Make sure it rendered correctly with the updated scrap
            // wait(100000);

        }
    }
}
