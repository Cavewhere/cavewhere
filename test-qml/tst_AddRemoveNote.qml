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

        function test_addNotes() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

                    function toUrl(filePath) {
                                return Qt.url("file://" + filePath)
                    }

            //Copy test data to another
            let phakeCavePath = toUrl(TestHelper.copyToTempDir("://datasets/test_cwTextureUploadTask/PhakeCave.PNG"));
            let bonesPath = toUrl(TestHelper.copyToTempDir("://datasets/test_cwSurveyNotesConcatModel/bones.glb"));
                    // let bonesPath = toUrl(TestHelper.copyToTempDir("/Users/cave/Downloads/9_8_2025 2.glb"));

            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery");
            noteGallery.imagesAdded([phakeCavePath, bonesPath]);

            let noteGalleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView");
            tryVerify(() => { return noteGalleryView.count === 3});

            let noteImage2_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage2")
            mouseClick(noteImage2_obj1)

                    //Zoom into the the model
            let turnTable = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->turnTableInteraction")
                    for(let i = 0; i < 75; i++) {
                                wait(20);
                                mouseWheel(turnTable,
                                           turnTable.width / 2.0, turnTable.height / 2.0,
                                           0, 50);
                    }

            let carpetButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(carpetButton)

                    let addStationButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addScrapStation")
                    mouseClick(addStationButton)


                    let turnTableInteraction_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->turnTableInteraction")
                    mouseClick(turnTableInteraction_obj2, 150.418, 330.957)



            wait(100000)
        }

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
