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

        function init() {
            rootId.width = 1600
            TestHelper.loadProjectFromZip(RootData.project, TestHelper.testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            let imagePath = TestHelper.copyToTempDirUrl(TestHelper.testcasesDatasetPath("test_cwTextureUploadTask/PhakeCave.PNG"));
            let lidarPath = TestHelper.copyToTempDirUrl(TestHelper.testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));

            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery");
            noteGallery.imagesAdded([imagePath, lidarPath]);

            let noteGalleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView");
            tryVerify(() => { return noteGalleryView.count === 2});

            let lidarThumb = null;
            tryVerify(() => {
                lidarThumb = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView->noteImage1");
                return lidarThumb !== null;
            }, 1000, "lidarThumb (noteImage1) should exist after images added")
            mouseClick(lidarThumb);

            let lidarViewer = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId");
            tryVerify(() => { return lidarViewer.scene.gltf.status === RenderGLTF.Ready }, 10000);

            RootData.futureManagerModel.waitForFinished();

            // The mesh BVH is built on a worker thread after the GLTF is Ready, so a
            // pick arriving before it finishes silently misses. Wait until the mesh is
            // actually pickable so the interaction tests don't race the build.
            let turnTable = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->turnTableInteraction");
            tryVerify(() => { return turnTable.pick(Qt.point(387, 257)).hit }, 10000, "LiDAR mesh should be pickable");
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
            let carpetButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->mainButtonArea->carpetButtonId")
            mouseClick(carpetButton)

            let addStationButton = null
            tryVerify(() => {
                          addStationButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addScrapStation")
                          return addStationButton !== null && addStationButton.visible
                      })
            mouseClick(addStationButton)

            let addStation_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARAddStationInteraction")
            mouseClick(addStation_obj1, 405, 239)

            let stationNameInput_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARStation_0->coreTextInput")
            stationNameInput_obj1.openEditor();
            keyClick(54, 0) //6
            keyClick(16777220, 0) //Return

            let addStation_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARAddStationInteraction")
            mouseClick(addStation_obj2, 564, 479)

            let stationNameInput_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARStation_1->coreTextInput")
            stationNameInput_obj2.openEditor();
            keyClick(55, 0) //7
            keyClick(16777220, 0) //Return

            //Add the 3rd station
            let addStation_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARAddStationInteraction")
            mouseClick(addStation_obj3, 252, 179)
            let stationNameInput_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARStation_2->coreTextInput")
            stationNameInput_obj3.openEditor();
            keyClick(53, 0) //5
            keyClick(16777220, 0) //Return

            let northFieldObject = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->northField")
            tryVerify(() => { return northFieldObject.text === "269.3" })

            //Move station 6
            let selection = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->selectButton")
            mouseClick(selection)

            let turnTableInteraction_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->turnTableInteraction")

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

            tryVerify(() => {
                          let current3D = stationIconHandler.position3D !== undefined ? stationIconHandler.position3D : stationItem.position3D
                          let mapping = stationIconHandler.mapToItem(stationIconHandler.parentView, stationIconHandler.parent.x, stationIconHandler.parent.y)
                          let pick = turnTableInteraction_obj3.pick(Qt.point(stationIconHandler.x, stationIconHandler.y))
                          return pick.hit && !vecClose(current3D, oldVec, 0.0001) && vecClose(current3D, pick.pointWorld, 0.0001)
                      })

            let back = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->backButton")
            mouseClick(back, 15.7781, 23.1722)

            //Make sure that the transform is visible
                    tryVerify(() => {
                                          let transform = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor")
                                          return transform.visible === true
                              })
        }

        function test_northInteraction() {
            //Turn off auto calculate
            let _obj1 = null
            tryVerify(() => {
                          _obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->checkBox")
                          return _obj1 !== null
                      })
            _obj1.checked = false;


            //Go through the tool
            let northToolButton_obj2 = null
            tryVerify(() => {
                          northToolButton_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->northToolButton")
                          return northToolButton_obj2 !== null && northToolButton_obj2.visible
                      })
            mouseClick(northToolButton_obj2)

            let noteLiDARNorthInteraction_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARNorthInteraction")
            mouseClick(noteLiDARNorthInteraction_obj3, 475, 499)
            mouseClick(noteLiDARNorthInteraction_obj3, 473.191, 241.352)

            //Escape out of the tool
            keyClick(16777216, 0) //Esc

            //Make sure the tool works again
            //Go through the tool
            tryVerify(() => {
                          northToolButton_obj2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->northToolButton")
                          return northToolButton_obj2 !== null && northToolButton_obj2.visible
                      })
            mouseClick(northToolButton_obj2)

            tryVerify(() => {
                          noteLiDARNorthInteraction_obj3 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARNorthInteraction")
                          return noteLiDARNorthInteraction_obj3 !== null
                      })
            mouseClick(noteLiDARNorthInteraction_obj3, 473.191, 241.352)
            mouseMove(noteLiDARNorthInteraction_obj3, 475, 499, 100)
            mouseClick(noteLiDARNorthInteraction_obj3, 475, 499)

            let azimuth = null
            tryVerify(() => {
                          azimuth = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARNorthInteraction->valueInput")
                          return azimuth !== null
                      })

            azimuth.openEditor()
            // wait() needed — openEditor() activates the text input asynchronously;
            // activeFocus may not be reliable under --platform offscreen
            wait(100)

            keyClick(54, 0) //6
            keyClick(50, 0) //2
            keyClick(16777220, 0) //Return

            tryVerify(() => { return azimuth.text === "62.0"; })


            let apply = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARNorthInteraction->apply")
            mouseClick(apply)


            let northFieldObject = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->northField")
            tryVerify(() => {
                          return northFieldObject.text === "117.6"
                      })

            let trip = RootData.region.cave(0).trip(0)
            trip.calibration.declinationManual = 12.5
            tryVerify(() => {
                          return northFieldObject.text === "117.6"
                      })

        }

        function test_scaleInteraction() {
            let transformEditor = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor")
            let noteTransform = transformEditor.noteTransform
            verify(noteTransform !== null)

            let setLengthButton = null
            tryVerify(() => {
                          setLengthButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->setLengthButton")
                          return setLengthButton !== null
                      })
            mouseClick(setLengthButton)

            let scaleInteraction = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARScaleInteraction")
            tryVerify(() => {
                if (!scaleInteraction.visible) {
                    mouseClick(setLengthButton)
                    return false
                }
                return true
            }, 3000, "scaleInteraction should be visible after clicking setLengthButton")
            mouseClick(scaleInteraction, 387, 257)
            mouseClick(scaleInteraction, 473.699, 255.484)

            tryVerify(() => { return scaleInteraction.measuredValue > 1.0 })
            let measured = scaleInteraction.measuredValue

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

            let setUpToolButton_obj1 = null
            tryVerify(() => {
                          setUpToolButton_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARTransformEditor->setUpToolButton")
                          return setUpToolButton_obj1 !== null && setUpToolButton_obj1.visible
                      })
            mouseClick(setUpToolButton_obj1)

            let noteLiDARUpInteraction_obj1 = null
            tryVerify(() => {
                          noteLiDARUpInteraction_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId->noteLiDARUpInteraction")
                          return noteLiDARUpInteraction_obj1 !== null
                      })
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
