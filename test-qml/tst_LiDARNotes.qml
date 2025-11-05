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

        function toUrl(filePath) {
            return Qt.url("file://" + filePath)
        }

        function init() {
            TestHelper.loadProjectFromZip(RootData.project, "://datasets/lidarProjects/jaws of the beast.zip");
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

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
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function vecClose(a, b, epsilon) {
            if (a === undefined || b === undefined) {
                return false;
            }
            epsilon = epsilon || 0.001;
            return Math.abs(a.x - b.x) < epsilon &&
                   Math.abs(a.y - b.y) < epsilon &&
                   Math.abs(a.z - b.z) < epsilon;
        }

        function test_lidarCarpeting() {
            let carpetButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            mouseClick(carpetButton)

            wait(200)

            let addStationButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addScrapStation")
            mouseClick(addStationButton)


            let turnTableInteraction_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->turnTableInteraction")
            mouseClick(turnTableInteraction_obj2, 203.117, 244.969)

            let stationNameInput_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARStation_0->coreTextInput")
            stationNameInput_obj1.openEditor();
            keyClick(54, 0) //6
            keyClick(16777220, 0) //Return
            // wait(10000)

            let turnTableInteraction_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->turnTableInteraction")
            mouseClick(turnTableInteraction_obj3, 333.148, 541.453)

            let stationNameInput_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARStation_1->coreTextInput")
            stationNameInput_obj2.openEditor();
            keyClick(55, 0) //7
            keyClick(16777220, 0) //Return

            //Add the 3rd station
            mouseClick(turnTableInteraction_obj3, 58.7891, 196.809)
            let stationNameInput_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARStation_2->coreTextInput")
            stationNameInput_obj3.openEditor();
            keyClick(53, 0) //5
            keyClick(16777220, 0) //Return

            let northFieldObject = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->northField")
            tryVerify(() => { return northFieldObject.text === "272.7" })

            //Move station 6
            let selection = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->selectButton")
            mouseClick(selection)


            //Click and drag the station icon
            let stationIconHandler = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARStation_0")
            let stationItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARStation_0")

            let handlerPosition3D = stationIconHandler.position3D !== undefined ? stationIconHandler.position3D : stationItem.position3D
            verify(handlerPosition3D !== undefined)

            let handlerPoint2D = Qt.point(stationItem.x, stationItem.y)
            let initialPick = turnTableInteraction_obj3.pick(handlerPoint2D)
            verify(initialPick.hit)

            verify(vecClose(handlerPosition3D, initialPick.pointWorld, 0.0001))

            //This copyies the value instead of using a javascript reference that updates
            let oldVec = Qt.vector3d(handlerPosition3D.x, handlerPosition3D.y, handlerPosition3D.z)


            mouseDrag(stationIconHandler,
                      stationIconHandler.width / 2.0,
                      stationIconHandler.height / 2.0,
                      40, 30,
                      Qt.LeftButton, Qt.NoModifier, 100)

                    wait(100);

            tryVerify(() => {
                let current3D = stationIconHandler.position3D !== undefined ? stationIconHandler.position3D : stationItem.position3D
                let mapping = stationIconHandler.mapToItem(stationIconHandler.parentView, stationIconHandler.parent.x, stationIconHandler.parent.y)
                let pick = turnTableInteraction_obj3.pick(Qt.point(stationIconHandler.x, stationIconHandler.y))
                return pick.hit && !vecClose(current3D, oldVec, 0.0001) && vecClose(current3D, pick.pointWorld, 0.0001)
            })
        }

        function test_northInteraction() {
            wait(100)
        }

        function test_scaleInteraction() {
            wait(100)
        }

        function test_upInteraction() {
            wait(100)
        }
    }
}
