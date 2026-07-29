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
    }
}
