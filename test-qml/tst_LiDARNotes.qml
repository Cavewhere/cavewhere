import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "LiDARNotes"
        when: windowShown

        function test_prepareLiDARNoteSkeleton() {
            TestHelper.loadProjectFromZip(RootData.project, "://datasets/lidarProjects/jaws of the beast.zip");
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            function toUrl(filePath) {
                return Qt.url("file://" + filePath)
            }

            let imagePath = toUrl(TestHelper.copyToTempDir("://datasets/test_cwTextureUploadTask/PhakeCave.PNG"));
            let lidarPath = toUrl(TestHelper.copyToTempDir("://datasets/lidarProjects/9_15_2025 3.glb"));

            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery");
            noteGallery.imagesAdded([imagePath, lidarPath]);

            let noteGalleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView");
            tryVerify(() => { return noteGalleryView.count === 2});

            wait(50)

            let lidarThumb = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage1");
            mouseClick(lidarThumb);

            let lidarViewer = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId");
            tryVerify(() => { return lidarViewer.scene.gltf.status === RenderGLTF.Ready }, 10000);

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
    }
}
