import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "NoteOutsideScrapWarning"
        when: windowShown

        function init() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"));
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"
            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "tripPage" });

            // The dataset's existing note has a scrap; add a fresh image so we get
            // a note with zero scraps to exercise the warning UI.
            let phakeCavePath = TestHelper.copyToTempDirUrl(TestHelper.testcasesDatasetPath("test_cwTextureUploadTask/PhakeCave.PNG"));
            let noteGallery = findNoteGallery();
            noteGallery.imagesAdded([phakeCavePath]);

            let galleryView = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->galleryView");
            tryVerify(() => { return galleryView.count === 2 });
            tryCompare(galleryView, "currentIndex", 1);

            let carpetButton = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->mainButtonArea->carpetButtonId")
            mouseClick(carpetButton);
            // The "" → "SELECT" transition includes PropertyAnimations that
            // reposition the toolbar; without this wait, clicks on the add-mode
            // buttons land before they finish moving (same as tst_ScrapInteraction).
            wait(500);

            let noteArea = findNoteArea();
            tryCompare(noteArea, "scrapCount", 0);
        }

        function cleanup() {
            RootData.project.newProject();
            RootData.futureManagerModel.waitForFinished();
        }

        function findNoteGallery() {
            return ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery");
        }

        function findAddScrapHint() {
            return ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->addScrapHint");
        }

        function findNoteArea() {
            return ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea");
        }

        function clickToolbarButton(objectName) {
            let button = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->" + objectName);
            verify(button !== null, "toolbar button " + objectName + " not found");
            mouseClick(button);
        }

        function test_helpQuoteBoxHiddenInSelectMode() {
            // Visibility is gated on ADD-STATION / ADD-LEAD, not just scrapCount.
            let hint = findAddScrapHint();
            verify(hint !== null, "addScrapHint not found");
            verify(hint.visible === false);
        }

        function test_helpQuoteBoxVisibleInAddStationMode() {
            clickToolbarButton("addScrapStation");
            let hint = findAddScrapHint();
            tryVerify(() => { return hint.visible === true; });
        }

        function test_helpQuoteBoxVisibleInAddLeadMode() {
            clickToolbarButton("addLeads");
            let hint = findAddScrapHint();
            tryVerify(() => { return hint.visible === true; });
        }

        function test_helpQuoteBoxHiddenInAddScrapMode() {
            clickToolbarButton("addScrapStation");
            let hint = findAddScrapHint();
            tryVerify(() => { return hint.visible === true; });

            clickToolbarButton("addScrapButton");
            tryVerify(() => { return hint.visible === false; });
        }

        function test_errorHelpBoxShowsOnClickOutsideScrap() {
            clickToolbarButton("addScrapStation");

            let imageId = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId");
            verify(imageId !== null, "imageId not found");

            mouseClick(imageId, imageId.width / 2.0, imageId.height / 2.0);

            let warning = findChild(findNoteArea(), "outsideScrapStationWarning");
            tryVerify(() => { return warning !== null && warning.visible === true; });
            compare(warning.text, "Stations must be placed inside a scrap");
        }

        function test_errorHelpBoxShowsOnLeadClickOutsideScrap() {
            clickToolbarButton("addLeads");

            let imageId = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId");
            verify(imageId !== null);

            mouseClick(imageId, imageId.width / 2.0, imageId.height / 2.0);

            let warning = findChild(findNoteArea(), "outsideScrapLeadWarning");
            tryVerify(() => { return warning !== null && warning.visible === true; });
            compare(warning.text, "Leads must be placed inside a scrap");
        }
    }
}
