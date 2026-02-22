import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "TripSync"
        when: windowShown

        function tryVerifyWithDiagnostics(predicate, timeoutMs, label, onPending) {
            tryVerify(function() {
                return predicate()
            }, timeoutMs)
        }

        function openTripPage(caveName, tripName) {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + caveName + "/Trip=" + tripName
            tryVerifyWithDiagnostics(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
            }, 10000, "openTripPage")
        }

        function verifyStillOnTripPage(tripPageAddress) {
            tryCompare(RootData.pageSelectionModel, "currentPageAddress", tripPageAddress)
            tryVerifyWithDiagnostics(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
            }, 10000, "verifyStillOnTripPage")
        }

        function setTripDate(dateInput, newDateText) {
            dateInput.openEditor()
            keyClick(65, Qt.ControlModifier) // Ctrl+A
            for (let i = 0; i < newDateText.length; ++i) {
                keyClick(newDateText.charAt(i))
            }
            keyClick(16777220, 0) // Return
        }

        function setClickTextInput(input, newText) {
            input.openEditor()
            keyClick(65, Qt.ControlModifier) // Ctrl+A
            for (let i = 0; i < newText.length; ++i) {
                keyClick(newText.charAt(i))
            }
            keyClick(16777220, 0) // Return
        }

        function normalizeDecimalText(textValue) {
            let value = Number(textValue)
            if (!Number.isFinite(value)) {
                return String(textValue)
            }
            return value.toFixed(2)
        }

        function loadFixtureAndOpenFirstTrip() {
            let fixture = TestHelper.createLocalSyncFixtureWithLfsServer()
            compare(fixture.errorMessage, "")
            verify(fixture.projectFilePath !== "")
            verify(fixture.remoteRepoPath !== "")
            verify(fixture.lfsEndpoint !== "")
            compare(TestHelper.fileExists(TestHelper.toLocalUrl(fixture.projectFilePath)), true)

            TestHelper.loadProjectFromPath(RootData.project, fixture.projectFilePath)
            tryVerify(() => {
                return RootData.region.caveCount > 0
            }, 10000)
            let cave = null
            let trip = null
            for (let i = 0; i < RootData.region.caveCount; ++i) {
                let candidateCave = RootData.region.cave(i)
                if (candidateCave !== null && candidateCave.rowCount() > 0) {
                    cave = candidateCave
                    trip = candidateCave.trip(0)
                    break
                }
            }
            verify(cave !== null)
            verify(trip !== null)
            let caveName = String(cave.name)
            let tripName = String(trip.name)
            verify(caveName.length > 0)
            verify(tripName.length > 0)
            openTripPage(caveName, tripName)
            return {
                tripPageAddress: "Source/Data/Cave=" + caveName + "/Trip=" + tripName
            }
        }

        function waitForProjectSyncToFinish() {
            tryVerifyWithDiagnostics(() => {
                return RootData.project.syncInProgress === false
            }, 3000, "waitForProjectSyncToFinish")
        }

        function runTripSyncRoundTrip(tripPageAddress, getter, setter, nextValue) {
            const verifyEditedValueTimeoutMs = 1000
            const verifyBaselineAfterCheckoutTimeoutMs = 1000
            const verifyResyncedValueTimeoutMs = 1000

            let baselineValue = String(getter())
            verify(baselineValue.length > 0)

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            let syncedValue = String(nextValue(baselineValue))
            setter(syncedValue)
            tryVerifyWithDiagnostics(() => {
                return String(getter()) === syncedValue
            }, verifyEditedValueTimeoutMs, "verify value after local setter")

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")

            verifyStillOnTripPage(tripPageAddress)
            tryVerifyWithDiagnostics(() => {
                return String(getter()) === baselineValue
            }, verifyBaselineAfterCheckoutTimeoutMs, "verify baseline after checkout")

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitB === commitC)

            verifyStillOnTripPage(tripPageAddress)
            tryVerifyWithDiagnostics(() => {
                return String(getter()) === syncedValue
            }, verifyResyncedValueTimeoutMs, "verify synced value after second sync")
        }

        function test_tripDateSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let findTripDateInput = function() {
                let dateInput = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->tripDate")
                verify(dateInput !== null)
                return dateInput
            }

            runTripSyncRoundTrip(
                context.tripPageAddress,
                function() {
                    return findTripDateInput().text
                },
                function(newValue) {
                    setTripDate(findTripDateInput(), newValue)
                },
                function(baselineValue) {
                    return baselineValue === "2026-01-15" ? "2026-01-16" : "2026-01-15"
                }
            )
        }

        function test_tripCalibrationSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let findCalibrationControls = function() {
                let rootChain = "rootId->tripPage->view"
                let declinationEdit = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->declinationEdit")
                let frontEditor = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->frontSightCalibrationEditor")
                let backEditor = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->backSightCalibrationEditor")
                let frontCompassEdit = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->frontSightCalibrationEditor->frontCompassCalibrationEdit")
                let frontClinoEdit = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->frontSightCalibrationEditor->frontClinoCalibrationEdit")
                let backCompassEdit = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->backSightCalibrationEditor->backCompassCalibrationEdit")
                let backClinoEdit = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->backSightCalibrationEditor->backClinoCalibrationEdit")
                let backwardsCompassCheck = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->frontSightCalibrationEditor->backwardsCompassCheck")
                let backwardsClinoCheck = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->frontSightCalibrationEditor->backwardsClinoCheck")
                let correctedCompassCheck = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->backSightCalibrationEditor->correctedCompassCheck")
                let correctedClinoCheck = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->backSightCalibrationEditor->correctedClinoCheck")

                verify(declinationEdit !== null)
                verify(frontEditor !== null)
                verify(backEditor !== null)
                verify(frontCompassEdit !== null)
                verify(frontClinoEdit !== null)
                verify(backCompassEdit !== null)
                verify(backClinoEdit !== null)
                verify(backwardsCompassCheck !== null)
                verify(backwardsClinoCheck !== null)
                verify(correctedCompassCheck !== null)
                verify(correctedClinoCheck !== null)

                return {
                    declinationEdit: declinationEdit,
                    frontEditor: frontEditor,
                    backEditor: backEditor,
                    frontCompassEdit: frontCompassEdit,
                    frontClinoEdit: frontClinoEdit,
                    backCompassEdit: backCompassEdit,
                    backClinoEdit: backClinoEdit,
                    backwardsCompassCheck: backwardsCompassCheck,
                    backwardsClinoCheck: backwardsClinoCheck,
                    correctedCompassCheck: correctedCompassCheck,
                    correctedClinoCheck: correctedClinoCheck
                }
            }

            let snapshotCalibrationState = function() {
                let controls = findCalibrationControls()
                let modelBackSights = controls.backEditor.calibration
                                      ? (controls.backEditor.calibration.backSights === true)
                                      : false
                let state = JSON.stringify({
                    declination: normalizeDecimalText(controls.declinationEdit.text),
                    frontCompassCalibration: normalizeDecimalText(controls.frontCompassEdit.text),
                    frontClinoCalibration: normalizeDecimalText(controls.frontClinoEdit.text),
                    backCompassCalibration: normalizeDecimalText(controls.backCompassEdit.text),
                    backClinoCalibration: normalizeDecimalText(controls.backClinoEdit.text),
                    modelBackSights: modelBackSights,
                    frontSightsEnabled: controls.frontEditor.checked === true,
                    frontBackwardsCompass: controls.backwardsCompassCheck.checked === true,
                    frontBackwardsClino: controls.backwardsClinoCheck.checked === true,
                    backCorrectedCompass: controls.correctedCompassCheck.checked === true,
                    backCorrectedClino: controls.correctedClinoCheck.checked === true,
                    backSightsEnabled: controls.backEditor.checked === true
                })
                return state
            }

            let applyCalibrationState = function(stateJson) {
                let state = JSON.parse(stateJson)
                let controls = findCalibrationControls()
                let calibration = controls.frontEditor.calibration

                setClickTextInput(controls.declinationEdit, state.declination)
                setClickTextInput(controls.frontCompassEdit, state.frontCompassCalibration)
                setClickTextInput(controls.frontClinoEdit, state.frontClinoCalibration)
                setClickTextInput(controls.backCompassEdit, state.backCompassCalibration)
                setClickTextInput(controls.backClinoEdit, state.backClinoCalibration)

                // Update model properties to avoid breaking QML bindings on CheckBox.checked.
                calibration.correctedCompassFrontsight = state.frontBackwardsCompass
                calibration.correctedClinoFrontsight = state.frontBackwardsClino
                calibration.correctedCompassBacksight = state.backCorrectedCompass
                calibration.correctedClinoBacksight = state.backCorrectedClino
                calibration.frontSights = state.frontSightsEnabled
                calibration.backSights = state.backSightsEnabled
            }

            let nextCalibrationState = function(baselineStateJson) {
                let baseline = JSON.parse(baselineStateJson)
                return JSON.stringify({
                    declination: baseline.declination === "0.00" ? "1.50" : "0.00",
                    frontCompassCalibration: baseline.frontCompassCalibration === "0.00" ? "2.25" : "0.00",
                    frontClinoCalibration: baseline.frontClinoCalibration === "0.00" ? "-1.75" : "0.00",
                    backCompassCalibration: baseline.backCompassCalibration === "0.00" ? "1.25" : "0.00",
                    backClinoCalibration: baseline.backClinoCalibration === "0.00" ? "-0.80" : "0.00",
                    modelBackSights: !baseline.modelBackSights,
                    frontSightsEnabled: baseline.frontSightsEnabled,
                    frontBackwardsCompass: !baseline.frontBackwardsCompass,
                    frontBackwardsClino: !baseline.frontBackwardsClino,
                    backCorrectedCompass: !baseline.backCorrectedCompass,
                    backCorrectedClino: !baseline.backCorrectedClino,
                    backSightsEnabled: !baseline.backSightsEnabled
                })
            }

            let controlsForSpies = findCalibrationControls()
            compare(controlsForSpies.frontEditor.checked, true)

            runTripSyncRoundTrip(
                context.tripPageAddress,
                function() {
                    return snapshotCalibrationState()
                },
                function(stateJson) {
                    applyCalibrationState(stateJson)
                },
                function(baselineStateJson) {
                    return nextCalibrationState(baselineStateJson)
                }
            )

        }

        function test_tripFrontSightsDisableSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let findFrontSightState = function() {
                let rootChain = "rootId->tripPage->view"
                let frontEditor = ObjectFinder.findObjectByChain(mainWindow, rootChain + "->frontSightCalibrationEditor")
                verify(frontEditor !== null)
                verify(frontEditor.calibration !== null)
                return {
                    frontEditor: frontEditor,
                    calibration: frontEditor.calibration
                }
            }

            let snapshotFrontSightState = function() {
                let controls = findFrontSightState()
                return JSON.stringify({
                    frontSightsEnabled: controls.frontEditor.checked === true,
                    modelFrontSights: controls.calibration.frontSights === true
                })
            }

            let applyFrontSightState = function(stateJson) {
                let state = JSON.parse(stateJson)
                let controls = findFrontSightState()
                controls.calibration.frontSights = state.frontSightsEnabled
            }

            let nextFrontSightState = function(baselineStateJson) {
                let baseline = JSON.parse(baselineStateJson)
                verify(baseline.frontSightsEnabled === true)
                verify(baseline.modelFrontSights === true)
                return JSON.stringify({
                    frontSightsEnabled: false,
                    modelFrontSights: false
                })
            }

            runTripSyncRoundTrip(
                context.tripPageAddress,
                function() {
                    return snapshotFrontSightState()
                },
                function(stateJson) {
                    applyFrontSightState(stateJson)
                },
                function(baselineStateJson) {
                    return nextFrontSightState(baselineStateJson)
                }
            )

        }
    }
}
