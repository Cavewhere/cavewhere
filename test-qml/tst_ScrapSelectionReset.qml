import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "ScrapSelectionReset"
        when: windowShown

        function openTripNotes() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"
            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "tripPage" });

            let carpetButton = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(carpetButton);
            wait(200);

            let noteArea = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea");
            tryVerify(() => { return noteArea !== null; });
            return noteArea;
        }

        function test_selectionClearsOnReload() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_ScrapInteraction/projectedProfile.cw");

            let noteArea = openTripNotes();
            let scrapView = findChild(noteArea, "scrapViewId");
            tryVerify(() => { return scrapView !== null; });

            let imageId = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageId, 470.703, 608.133);
            tryVerify(() => { return scrapView.selectedScrapItem !== null; });

            let previousScrap = scrapView.selectedScrapItem.scrap;

            // RootData.futureManagerModel.waitForFinished();
            RootData.project.newProject();
            RootData.futureManagerModel.waitForFinished();

            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_ScrapInteraction/projectedProfile.cw");

            noteArea = openTripNotes();
            scrapView = findChild(noteArea, "scrapViewId");
            tryVerify(() => { return scrapView !== null; });

            verify(scrapView.selectedScrapItem === null);
            // verify(scrapView.selectedScrapItem.scrap !== previousScrap);

        }
    }
}
