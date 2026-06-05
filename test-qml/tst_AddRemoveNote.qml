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

        function init() {
                    // Wide enough that the trip page shows the inline note gallery
                    // (showGallery is gated on reaching breakpointFullGallery); the
                    // tests click note thumbnails and their remove buttons.
                    rootId.width = 1600
                    RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function test_addNotes() {
                    // TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"));
                    TestHelper.loadProjectFromZip(RootData.project, TestHelper.testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
                    RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault"

                    tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

                    // //Copy test data to another
                    let phakeCavePath = TestHelper.copyToTempDirUrl(TestHelper.testcasesDatasetPath("test_cwTextureUploadTask/PhakeCave.PNG"));
                    let bonesPath = TestHelper.copyToTempDirUrl(TestHelper.testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));

                    let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery");
                    noteGallery.imagesAdded([phakeCavePath, bonesPath]);

                    let noteGalleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView");
                    tryVerify(() => { return noteGalleryView.count === 2});

                    waitForRendering(noteGalleryView)

                    let noteImage2_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage1")
                    mouseClick(noteImage2_obj1)

                    let noteLiDARItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId")

                    tryVerify(() => { return noteLiDARItem.scene.gltf.status === RenderGLTF.Ready }, 10000)

                    let noteImageItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage1->noteImageItem")
                    tryVerify(() => { return noteImageItem !== null })
                    tryVerify(() => { return noteImageItem.status === Image.Ready })

        }

        function test_autoSelectNewNote() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"));
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery");
            let noteGalleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView");

            tryVerify(() => { return noteGalleryView.count === 1 });
            compare(noteGalleryView.currentIndex, 0);

            let phakeCavePath = TestHelper.copyToTempDirUrl(TestHelper.testcasesDatasetPath("test_cwTextureUploadTask/PhakeCave.PNG"));
            noteGallery.addFiles([phakeCavePath]);

            tryVerify(() => { return noteGalleryView.count === 2 });
            tryCompare(noteGalleryView, "currentIndex", 1);
        }

        function test_removeNote() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"));
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"

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

        function test_removeLidarNote() {
            TestHelper.loadProjectFromZip(RootData.project, TestHelper.testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault"

            tryVerify(() => { return RootData.pageView.currentPageItem !== null })
                    tryVerify(() => {return RootData.pageView.currentPageItem.objectName === "tripPage" });

            let phakeCavePath = TestHelper.copyToTempDirUrl(TestHelper.testcasesDatasetPath("test_cwTextureUploadTask/PhakeCave.PNG"));
            let bonesPath = TestHelper.copyToTempDirUrl(TestHelper.testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));

            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery");
            noteGallery.imagesAdded([phakeCavePath, bonesPath]);

            let noteGalleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView");
            tryVerify(() => { return noteGalleryView.count === 2});

            waitForRendering(noteGalleryView)

            let noteImage1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage1")
            mouseClick(noteImage1)

            let removeImageButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage1->removeImageButton")
            mouseClick(removeImageButton)

            let cancelChallenge = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage1->removeChallange")
            tryVerify(() => { return cancelChallenge.visible; });

            let removeButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage1->removeChallange->removeButton")
            mouseClick(removeButton)

            tryVerify(() => { return !cancelChallenge.visible; });
            tryVerify(() => { return noteGalleryView.count === 1});
        }
    }
}
