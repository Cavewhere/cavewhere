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
                    // TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
                    TestHelper.loadProjectFromZip(RootData.project, "://datasets/lidarProjects/jaws of the beast.zip");
                    RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault"

                    tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

                    function toUrl(filePath) {
                                return Qt.url("file://" + filePath)
                    }

                    // //Copy test data to another
                    let phakeCavePath = toUrl(TestHelper.copyToTempDir("://datasets/test_cwTextureUploadTask/PhakeCave.PNG"));
                    let bonesPath = toUrl(TestHelper.copyToTempDir("://datasets/lidarProjects/9_15_2025 3.glb"));

                    let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery");
                    noteGallery.imagesAdded([phakeCavePath, bonesPath]);

                    let noteGalleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView");
                    tryVerify(() => { return noteGalleryView.count === 2});

                    waitForRendering(noteGalleryView)

                    let noteImage2_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage1")
                    mouseClick(noteImage2_obj1)

                    let noteLiDARItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId")

                    tryVerify(() => { return noteLiDARItem.scene.gltf.status === RenderGLTF.Ready }, 10000)

                    //         //Zoom into the the model
                    // let turnTable = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->turnTableInteraction")
                    //         for(let i = 0; i < 40; i++) {
                    //                     wait(20);
                    //                     mouseWheel(turnTable,
                    //                                turnTable.width / 2.0, turnTable.height / 2.0,
                    //                                0, 50);
                    //         }

                    let carpetButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
                    mouseClick(carpetButton)

                    wait(200)

                    let addStationButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addScrapStation")
                    mouseClick(addStationButton)


                    let turnTableInteraction_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->turnTableInteraction")
                    mouseClick(turnTableInteraction_obj2, 203.117, 244.969)

                    let stationIconHandler_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARStation_0->coreTextInput")
                    stationIconHandler_obj1.openEditor();
                    keyClick(54, 0) //6
                    keyClick(16777220, 0) //Return
                    // wait(10000)

                    let turnTableInteraction_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->turnTableInteraction")
                    mouseClick(turnTableInteraction_obj3, 333.148, 541.453)

                    let stationIconHandler_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARStation_1->coreTextInput")
                    stationIconHandler_obj2.openEditor();
                    keyClick(55, 0) //7
                    keyClick(16777220, 0) //Return

                    //Add the 3rd station
                    mouseClick(turnTableInteraction_obj3, 58.7891, 196.809)
                    let stationIconHandler_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARStation_2->coreTextInput")
                    stationIconHandler_obj3.openEditor();
                    keyClick(53, 0) //5
                    keyClick(16777220, 0) //Return

                    let northFieldObject = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->northField")
                    tryVerify(() => { return northFieldObject.text === "272.7" })

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
