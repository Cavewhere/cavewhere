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

            //Turn off auto calculate
            let _obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->checkBox")
            _obj1.checked = false;


            //Go through the tool
            let northToolButton_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->northToolButton")
            verify(northToolButton_obj2.visible)
            mouseClick(northToolButton_obj2)

            let noteLiDARNorthInteraction_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARNorthInteraction")
            mouseClick(noteLiDARNorthInteraction_obj3, 203.246, 244.637)
            mouseClick(noteLiDARNorthInteraction_obj3, 319.91, 514.371)

            //Escape out of the tool
            keyClick(16777216, 0) //Esc

            wait(100)

            //Make sure the tool works again
            //Go through the tool
            northToolButton_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->northToolButton")
            mouseClick(northToolButton_obj2)

            wait(100)

            noteLiDARNorthInteraction_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARNorthInteraction")
            mouseClick(noteLiDARNorthInteraction_obj3, 319.91, 514.371)
            mouseMove(noteLiDARNorthInteraction_obj3, 203.246, 244.637, 100)
            mouseClick(noteLiDARNorthInteraction_obj3, 203.246, 244.637)

            wait(100)

            let azimuth = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARNorthInteraction->valueInput")

            mouseClick(azimuth)

            wait(100)

            waitForRendering(azimuth);

            keyClick(54, 0) //6
            keyClick(50, 0) //2
            keyClick(16777220, 0) //Return

            wait(100)

            tryVerify(() => { return azimuth.text === "62.0"; })


            let apply = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARNorthInteraction->apply")
            mouseClick(apply)


            let northFieldObject = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->northField")
            tryVerify(() => {
                          return northFieldObject.text === "274.5"
                      })

        }

        function test_scaleInteraction() {
            let transformEditor = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor")
            let noteTransform = transformEditor.noteTransform
            verify(noteTransform !== null)

            wait(100)

            let setLengthButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->setLengthButton")
            mouseClick(setLengthButton)

            wait(100)

            let scaleInteraction = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARScaleInteraction")
            tryVerify(() => scaleInteraction.visible)
            mouseClick(scaleInteraction, 203.246, 244.637)
            mouseClick(scaleInteraction, 319.91, 514.371)

            wait(100)

            let measured = scaleInteraction.measuredValue
            verify(measured > 1.0) //if this is 1.0 the measuredValue isn't right

            let lengthInput = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARScaleInteraction->scaleLengthInput->coreTextInput")
            mouseClick(lengthInput)

            keyClick(49, 0) //1
            keyClick(48, 0) //0
            keyClick(16777220, 0) //Return

            let applyButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARScaleInteraction->apply")
            mouseClick(applyButton)

            let expectedScale = measured / 10.0

            tryVerify(() => {
                          return Math.abs(noteTransform.scale - expectedScale) < 0.0001
                      })

            tryVerify(() => {
                          let numerator = noteTransform.scaleNumerator
                          let denominator = noteTransform.scaleDenominator
                          return numerator.unit === Units.LengthUnitless &&
                                  Math.abs(numerator.value - measured) < 0.0001 &&
                                  Math.abs(denominator.value - 10.0) < 0.0001
                      })
        }

        function test_upInteraction() {
            let noteTransform = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor").noteTransform
            let oldUp = noteTransform.up;

            let upModeCombo_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->upModeCombo")
            let index = upModeCombo_obj1.find("Custom", Qt.MatchExactly)
            upModeCombo_obj1.currentIndex = index
            upModeCombo_obj1.activated(index)

            verify(upModeCombo_obj1.displayText === "Custom");

            wait(100)

            let setUpToolButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->setUpToolButton")
            mouseClick(setUpToolButton_obj1)

            wait(100)

            let noteLiDARUpInteraction_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARUpInteraction")
            mouseClick(noteLiDARUpInteraction_obj1, 60.8242, 396.969)
            mouseClick(noteLiDARUpInteraction_obj1, 56.8516, 300.746)

            let newUp = noteTransform.up
            let dot = Math.abs(oldUp.scalar * newUp.scalar + oldUp.x * newUp.x + oldUp.y * newUp.y + oldUp.z * newUp.z)
            dot = Math.min(1.0, Math.max(-1.0, dot))
            let angleDiff = 2.0 * Math.acos(dot) * 180.0 / Math.PI
            verify(angleDiff < 0.1);
        }
    }
}
