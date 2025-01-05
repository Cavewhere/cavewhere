import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "AddRemoveNote"
        when: windowShown

        function test_removeNote() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            //Make sure we still have our image
            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView")
            verify(noteGallery.count === 1);

            //Click on the remove button
            let removeImageButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage0->removeImageButton")
            mouseClick(removeImageButton_obj1)

            //Make sure the cancel challange is visible
            let cancelChallenge = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage0->removeChallange")
            tryVerify(() => { return cancelChallenge.visible; });

            //Click on cancel button
            let cancelButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage0->removeChallange->cancelButton")
            mouseClick(cancelButton)

            //Make sure cancel challange is hidden
            tryVerify(() => { return !cancelChallenge.visible; });

            //Make sure we still have our image
            verify(noteGallery.count === 1);

            //Click on the remove button again
            mouseClick(removeImageButton_obj1)

            //Make sure cancel challange is visible
            tryVerify(() => { return cancelChallenge.visible; });

            //Click on the remove button
            let removeButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage0->removeChallange->removeButton")
            mouseClick(removeButton)

            //Make sure cancel challange is hidden
            tryVerify(() => { return !cancelChallenge.visible; });

            //Make sure we have remove the image
            verify(noteGallery.count === 0);
            tryVerify(() => {return !noteGallery.visible});
        }
    }
}
