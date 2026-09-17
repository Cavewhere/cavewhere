import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    CWTestCase {
        id: testCaseId
        name: "ScrapScaleTripUnits"
        when: windowShown

        //This test leaves a loaded project on the trip page with scrap morphs in
        //flight; without this the next test file to run inherits both
        function cleanup() {
            RootData.pageSelectionModel.gotoPageByName(null, "View")
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "viewPage"
            }, 5000, "returned to the view page")
            RootData.newProject()
        }

        function findDescendantWhere(rootObject, predicate) {
            if (rootObject === null || rootObject === undefined) {
                return null
            }
            if (predicate(rootObject)) {
                return rootObject
            }

            let childCollections = []
            if (rootObject.children !== undefined && rootObject.children !== null) {
                childCollections.push(rootObject.children)
            }
            if (rootObject.contentItem !== undefined && rootObject.contentItem !== null) {
                childCollections.push([rootObject.contentItem])
            }

            for (let c = 0; c < childCollections.length; ++c) {
                let children = childCollections[c]
                for (let i = 0; i < children.length; ++i) {
                    let found = findDescendantWhere(children[i], predicate)
                    if (found !== null) {
                        return found
                    }
                }
            }

            return null
        }

        function unitInputIn(scaleInput, lengthInputName) {
            let lengthInput = findDescendantWhere(scaleInput, (child) => {
                return child !== null && child.objectName === lengthInputName
            })
            verify(lengthInput !== null, `found ${lengthInputName}`)

            let unitInput = findDescendantWhere(lengthInput, (child) => {
                return child !== null && child.objectName === "unitInput"
            })
            verify(unitInput !== null, `found unitInput in ${lengthInputName}`)
            return unitInput
        }

        //! The unit the editor is actually showing. UnitInput renders
        //! unitModel[menuId.selectedIndex], not unitModel[unit], so reading the
        //! label itself is the only way to catch the display falling behind the
        //! model — which is exactly the bug this test exists for.
        function displayedUnit(unitInput) {
            let label = findDescendantWhere(unitInput, (child) => {
                return child !== null && child.objectName === "unitLabel"
            })
            verify(label !== null, "found the unit label")
            return label.text.trim()
        }

        function checkScaleReads(scrap, onPaperUnitInput, inCaveUnitInput, onPaper, inCave) {
            let scaleObject = scrap.noteTransformation.scaleObject

            tryCompare(scaleObject.scaleNumerator, "unit", onPaper)
            tryCompare(scaleObject.scaleDenominator, "unit", inCave)

            tryVerify(() => { return displayedUnit(onPaperUnitInput) === Units.lengthUnitName(onPaper) },
                      5000,
                      `on paper reads ${Units.lengthUnitName(onPaper)}`)
            tryVerify(() => { return displayedUnit(inCaveUnitInput) === Units.lengthUnitName(inCave) },
                      5000,
                      `in cave reads ${Units.lengthUnitName(inCave)}`)
        }

        // An auto-calculated scale reads in the trip's survey unit, so toggling
        // the trip between meters and feet has to move both halves of the scale —
        // in the model and in the editor the user is looking at. The display half
        // is the easy one to lose: writing UnitInput.unit imperatively overwrites
        // its binding, and the scale then stays on whatever unit it was created
        // with no matter what the trip does.
        function test_tripDistanceUnitRelabelsAnAutoScale() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"));
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "tripPage" })

            let carpetButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->mainButtonArea->carpetButtonId")
            mouseClick(carpetButton)

            // wait() needed — the "" → "SELECT" transition includes PropertyAnimations
            // that reposition the toolbar; clicks miss during the animation
            wait(1000)

            let imageItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageItem, 475.801, 600.855)

            let noteArea = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea")
            let scrapView = findChild(noteArea, "scrapViewId")
            tryVerify(() => { return scrapView.selectedScrapItem !== null },
                      5000,
                      "the click selected a scrap")

            let scrap = scrapView.selectedScrapItem.scrap as Scrap
            verify(scrap !== null)

            //The fixture's scrap is manually scaled; auto is what this test is about
            scrap.calculateNoteTransform = true

            let noteTransformEditor = null
            tryVerify(() => {
                noteTransformEditor = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor")
                return noteTransformEditor !== null
            })

            let scaleInput = findDescendantWhere(noteTransformEditor, (child) => {
                return child !== null
                       && child.scaleObject !== undefined
                       && child.onPaperLabel !== undefined
                       && child.inCaveLabel !== undefined
            })
            verify(scaleInput !== null, "found the paper scale input")

            let onPaperUnitInput = unitInputIn(scaleInput, "onPaperLengthInput")
            let inCaveUnitInput = unitInputIn(scaleInput, "inCaveLengthInput")

            let trip = RootData.region.cave(0).trip(0)
            verify(trip !== null)
            compare(trip.calibration.distanceUnit, Units.Meters, "the fixture surveys in meters")

            checkScaleReads(scrap, onPaperUnitInput, inCaveUnitInput, Units.Centimeters, Units.Meters)

            trip.calibration.distanceUnit = Units.Feet
            checkScaleReads(scrap, onPaperUnitInput, inCaveUnitInput, Units.Inches, Units.Feet)

            trip.calibration.distanceUnit = Units.Meters
            checkScaleReads(scrap, onPaperUnitInput, inCaveUnitInput, Units.Centimeters, Units.Meters)
        }

        // The scale tool's "In cave length" starts in the trip's survey unit, and
        // re-reads it every time the tool opens, so a trip that switched units
        // since the last use gets the new one.
        function test_scaleToolStartsInTripDistanceUnit() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"));
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "tripPage" })

            let carpetButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->mainButtonArea->carpetButtonId")
            mouseClick(carpetButton)

            // wait() needed — the "" → "SELECT" transition includes PropertyAnimations
            // that reposition the toolbar; clicks miss during the animation
            wait(1000)

            let imageItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            mouseClick(imageItem, 475.801, 600.855)

            let noteArea = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea")
            let scrapView = findChild(noteArea, "scrapViewId")
            tryVerify(() => { return scrapView.selectedScrapItem !== null },
                      5000,
                      "the click selected a scrap")
            let scrap = scrapView.selectedScrapItem.scrap as Scrap
            verify(scrap !== null)

            let trip = RootData.region.cave(0).trip(0)
            trip.calibration.distanceUnit = Units.Feet

            let setLengthButton = null
            tryVerify(() => {
                setLengthButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->autoCalculate->setLengthButton")
                return setLengthButton !== null
            })
            mouseClick(setLengthButton)

            let scaleInteraction = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteScaleInteraction")
            let lengthUnitInput = unitInputIn(scaleInteraction, "lengthUnitValue")
            tryVerify(() => { return displayedUnit(lengthUnitInput) === Units.lengthUnitName(Units.Feet) },
                      5000,
                      "in cave length reads in the trip's feet")

            mouseClick(imageItem, 645.111, 738.692)
            mouseClick(imageItem, 777.563, 738.692)

            let lengthText = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteScaleInteraction->lengthUnitValue->coreTextInput")
            mouseClick(lengthText, 0.867188, 12.7031)

            keyClick(Qt.Key_1)
            keyClick(Qt.Key_0)
            keyClick(Qt.Key_Return)

            let done = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteScaleInteraction->doneButton->label")
            mouseClick(done)

            //Same clicks as tst_NoteScaleInteraction's 10 m (1:574.5), in feet
            const kMetersPerFoot = 0.3048
            tryFuzzyCompare(1.0 / scrap.noteTransformation.scale, 574.5 * kMetersPerFoot, 1.0,
                            `1.0 / ${scrap.noteTransformation.scale} is a 10 ft length`)

            trip.calibration.distanceUnit = Units.Meters
            tryVerify(() => { return !scaleInteraction.visible })
            mouseClick(setLengthButton)
            tryVerify(() => { return displayedUnit(lengthUnitInput) === Units.lengthUnitName(Units.Meters) },
                      5000,
                      "reopening the tool picks up the trip's switch to meters")
        }
    }
}
