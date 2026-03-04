import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder
import "SyncTestHelper.js" as SyncTestHelper

MainWindowTest {
    id: rootId

    TestCase {
        id: testCaseId
        name: "TripSync"
        when: windowShown

        function tryVerifyWithDiagnostics(predicate, timeoutMs, label, onPending) {
            SyncTestHelper.tryVerifyWithDiagnostics(testCaseId, predicate, timeoutMs, label, onPending)
        }

        function openTripPage(caveName, tripName) {
            SyncTestHelper.openTripPage(testCaseId, RootData, caveName, tripName)
        }

        function verifyStillOnTripPage(tripPageAddress) {
            SyncTestHelper.verifyStillOnTripPage(testCaseId, RootData, tripPageAddress)
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
            return SyncTestHelper.loadFixtureAndOpenFirstTrip(testCaseId, RootData, TestHelper)
        }

        function waitForProjectSyncToFinish() {
            SyncTestHelper.waitForProjectSyncToFinish(testCaseId, RootData)
        }

        function runTripSyncRoundTrip(tripPageAddress, getter, setter, nextValue, uiGetter) {
            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: tripPageAddress,
                getter: getter,
                setter: setter,
                nextValue: nextValue,
                uiGetter: uiGetter
            })
        }

        function teamMembersFromModel(teamModel) {
            let members = []
            for (let row = 0; row < teamModel.rowCount(); ++row) {
                let modelIndex = teamModel.index(row, 0)
                let name = String(teamModel.data(modelIndex, Team.NameRole))
                let jobsValue = teamModel.data(modelIndex, Team.JobsRole)
                let jobs = []
                if (Array.isArray(jobsValue)) {
                    for (let i = 0; i < jobsValue.length; ++i) {
                        jobs.push(String(jobsValue[i]))
                    }
                } else if (jobsValue !== null
                           && jobsValue !== undefined
                           && typeof jobsValue.length === "number") {
                    for (let j = 0; j < jobsValue.length; ++j) {
                        jobs.push(String(jobsValue[j]))
                    }
                } else if (jobsValue !== null && jobsValue !== undefined) {
                    jobs.push(String(jobsValue))
                }
                members.push({
                    name: name,
                    jobs: jobs
                })
            }
            return members
        }

        function snapshotTeamState(teamModel) {
            return JSON.stringify({
                members: teamMembersFromModel(teamModel)
            })
        }

        function applyTeamState(teamModel, stateJson) {
            let state = JSON.parse(stateJson)
            while (teamModel.rowCount() > 0) {
                teamModel.removeTeamMember(teamModel.rowCount() - 1)
            }

            for (let i = 0; i < state.members.length; ++i) {
                let member = state.members[i]
                teamModel.addTeamMember()
                let row = teamModel.rowCount() - 1
                teamModel.setData(row, Team.NameRole, member.name)
                teamModel.setData(row, Team.JobsRole, member.jobs)
            }
        }

        function findDescendantByObjectName(rootObject, objectName) {
            if (rootObject === null || rootObject === undefined) {
                return null
            }
            if (rootObject.objectName === objectName) {
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
                    let found = findDescendantByObjectName(children[i], objectName)
                    if (found !== null) {
                        return found
                    }
                }
            }

            return null
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

        function test_tripRenameSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let currentPageTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let tripId = String(currentPageTrip().id)
            let caveName = String(currentPageTrip().parentCave.name)
            verify(tripId.length > 0)
            verify(caveName.length > 0)

            let tripById = function() {
                for (let caveIndex = 0; caveIndex < RootData.region.caveCount; ++caveIndex) {
                    let cave = RootData.region.cave(caveIndex)
                    if (cave === null || cave === undefined) {
                        continue
                    }
                    for (let tripIndex = 0; tripIndex < cave.rowCount(); ++tripIndex) {
                        let trip = cave.trip(tripIndex)
                        if (trip !== null
                                && trip !== undefined
                                && String(trip.id) === tripId) {
                            return trip
                        }
                    }
                }
                return null
            }

            let tripPageAddressForName = function(tripName) {
                return "Source/Data/Cave=" + caveName + "/Trip=" + String(tripName)
            }

            let tripNameInput = function() {
                let input = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->tripNameText")
                verify(input !== null)
                return input
            }

            let renameTripViaUi = function(newName) {
                let input = tripNameInput()
                setClickTextInput(input, newName)
                tryVerifyWithDiagnostics(() => {
                    let trip = tripById()
                    return trip !== null
                           && String(trip.name) === String(newName)
                           && RootData.pageSelectionModel.currentPageAddress === tripPageAddressForName(newName)
                }, 10000, "verify trip renamed via UI")
            }

            let snapshotUiState = function() {
                let trip = tripById()
                verify(trip !== null)
                return {
                    modelTripName: String(trip.name),
                    tripNameText: String(tripNameInput().text),
                    currentPageAddress: String(RootData.pageSelectionModel.currentPageAddress),
                    currentPageObjectName: RootData.pageView.currentPageItem === null
                                           ? ""
                                           : String(RootData.pageView.currentPageItem.objectName)
                }
            }

            let expectedUiState = function(expectedTripName) {
                return {
                    modelTripName: String(expectedTripName),
                    tripNameText: String(expectedTripName),
                    currentPageAddress: tripPageAddressForName(expectedTripName),
                    currentPageObjectName: "tripPage"
                }
            }

            let verifyTripPageState = function(expectedTripName, label, timeoutMs) {
                tryVerifyWithDiagnostics(() => {
                    return JSON.stringify(snapshotUiState()) === JSON.stringify(expectedUiState(expectedTripName))
                }, timeoutMs, label)
            }

            let baselineName = String(currentPageTrip().name)
            let syncedName = baselineName === "Release 0.08"
                           ? "Release 0.08 Rename Sync"
                           : "Release 0.08"

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            verifyTripPageState(baselineName, "verify baseline trip rename UI", 10000)

            renameTripViaUi(syncedName)
            verifyTripPageState(syncedName, "verify renamed trip page state", 10000)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            verifyTripPageState(syncedName, "verify renamed trip page after first sync", 10000)

            console.log("About to Checked out!");
            // wait(1000);
            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")
            // wait(5000);
            console.log("Checked out!");

            verifyTripPageState(baselineName, "verify baseline trip page after checkout", 10000)

            console.log("About to final sync!");
            // wait(1000);
            verify(RootData.project.sync())
            waitForProjectSyncToFinish()
            // wait(5000)
            console.log("Final sync");

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitC === commitB)

            verifyTripPageState(syncedName, "verify renamed trip page after second sync", 10000)
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

        function test_tripTeamSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let currentTeamModel = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                let model = currentPageItem.currentTrip.team
                verify(model !== null)
                return model
            }

            let seededBaselineState = JSON.stringify({
                members: [
                    {
                        name: "Alice",
                        jobs: ["Lead", "Sketch", "Safety"]
                    },
                    {
                        name: "Bob",
                        jobs: ["Clino"]
                    }
                ]
            })
            let editedState = JSON.stringify({
                members: [
                    {
                        name: "Alice Updated",
                        jobs: ["Lead Updated", "Safety", "Data QC"]
                    },
                    {
                        name: "Cara",
                        jobs: ["Rigging", "Survey"]
                    }
                ]
            })

            applyTeamState(currentTeamModel(), seededBaselineState)
            compare(snapshotTeamState(currentTeamModel()), seededBaselineState)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            applyTeamState(currentTeamModel(), editedState)
            tryVerifyWithDiagnostics(() => {
                return snapshotTeamState(currentTeamModel()) === editedState
            }, 3000, "verify team value after local setter")

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            verifyStillOnTripPage(context.tripPageAddress)
            tryVerifyWithDiagnostics(() => {
                return snapshotTeamState(currentTeamModel()) === editedState
            }, 3000, "verify team synced value")

            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")

            verifyStillOnTripPage(context.tripPageAddress)
            tryVerifyWithDiagnostics(() => {
                return snapshotTeamState(currentTeamModel()) === seededBaselineState
            }, 3000, "verify baseline team value after checkout")

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitC === commitB)

            verifyStillOnTripPage(context.tripPageAddress)
            tryVerifyWithDiagnostics(() => {
                return snapshotTeamState(currentTeamModel()) === editedState
            }, 3000, "verify team synced value after second sync")
        }

        function test_tripTeamRemoveViaUiSync() {
            let context = loadFixtureAndOpenFirstTrip()
            let currentTeamModel = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                let model = currentPageItem.currentTrip.team
                verify(model !== null)
                return model
            }

            let seededBaselineState = JSON.stringify({
                members: [
                    {
                        name: "Alice",
                        jobs: ["Lead"]
                    },
                    {
                        name: "Bob",
                        jobs: ["Clino"]
                    },
                    {
                        name: "Cara",
                        jobs: ["Rigging"]
                    }
                ]
            })
            let expectedAfterRemovalState = JSON.stringify({
                members: [
                    {
                        name: "Alice",
                        jobs: ["Lead"]
                    },
                    {
                        name: "Cara",
                        jobs: ["Rigging"]
                    }
                ]
            })

            applyTeamState(currentTeamModel(), seededBaselineState)
            compare(snapshotTeamState(currentTeamModel()), seededBaselineState)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()
            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            verifyStillOnTripPage(context.tripPageAddress)

            let pageItem = RootData.pageView.currentPageItem
            let teamModel = currentTeamModel()
            let teamList = findDescendantWhere(pageItem, function(item) {
                return item !== null
                       && item !== undefined
                       && item.model !== undefined
                       && item.model === teamModel
                       && item.currentIndex !== undefined
                       && item.contentItem !== undefined
            })
            verify(teamList !== null)
            compare(teamList.count, 3)

            teamList.currentIndex = 1
            let rowDelegate = findDescendantWhere(teamList.contentItem, function(item) {
                return item !== null
                       && item !== undefined
                       && item.index !== undefined
                       && Number(item.index) === 1
                       && item.width !== undefined
                       && item.height !== undefined
            })
            verify(rowDelegate !== null)
            mouseClick(rowDelegate, 12, rowDelegate.height * 0.5, Qt.LeftButton)

            tryVerifyWithDiagnostics(() => {
                return snapshotTeamState(currentTeamModel()) === expectedAfterRemovalState
            }, 3000, "verify team state after UI removal")

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()
            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")
            verifyStillOnTripPage(context.tripPageAddress)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitC === commitB)

            verifyStillOnTripPage(context.tripPageAddress)
            let finalTeamState = snapshotTeamState(currentTeamModel())
            let deadline = Date.now() + 3000
            while (Date.now() < deadline && finalTeamState !== expectedAfterRemovalState) {
                wait(50)
                finalTeamState = snapshotTeamState(currentTeamModel())
            }
            compare(finalTeamState, expectedAfterRemovalState)
        }

        function test_tripChunkDataSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let currentTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let currentChunk = function() {
                let trip = currentTrip()
                verify(trip.chunkCount > 0)
                let chunk = trip.chunk(0)
                verify(chunk !== null)
                verify(chunk.stationCount >= 2)
                verify(chunk.shotCount >= 1)
                return chunk
            }

            let readingText = function(value) {
                if (value !== null && value !== undefined && value.value !== undefined) {
                    return String(value.value)
                }
                return String(value)
            }

            let snapshotChunkState = function() {
                let trip = currentTrip()
                let calibration = trip.calibration
                let chunk = currentChunk()
                return JSON.stringify({
                    backSightsEnabled: calibration.backSights === true,
                    stationName: String(chunk.data(SurveyChunk.StationNameRole, 1)),
                    distance: readingText(chunk.data(SurveyChunk.ShotDistanceRole, 0)),
                    compass: readingText(chunk.data(SurveyChunk.ShotCompassRole, 0)),
                    backCompass: readingText(chunk.data(SurveyChunk.ShotBackCompassRole, 0)),
                    clino: readingText(chunk.data(SurveyChunk.ShotClinoRole, 0)),
                    backClino: readingText(chunk.data(SurveyChunk.ShotBackClinoRole, 0)),
                    left: readingText(chunk.data(SurveyChunk.StationLeftRole, 0)),
                    right: readingText(chunk.data(SurveyChunk.StationRightRole, 0)),
                    up: readingText(chunk.data(SurveyChunk.StationUpRole, 0)),
                    down: readingText(chunk.data(SurveyChunk.StationDownRole, 0))
                })
            }

            let applyChunkState = function(stateJson) {
                let state = JSON.parse(stateJson)
                let trip = currentTrip()
                let calibration = trip.calibration
                let chunk = currentChunk()

                calibration.backSights = state.backSightsEnabled
                chunk.setData(SurveyChunk.StationNameRole, 1, state.stationName)
                chunk.setData(SurveyChunk.ShotDistanceRole, 0, state.distance)
                chunk.setData(SurveyChunk.ShotCompassRole, 0, state.compass)
                chunk.setData(SurveyChunk.ShotBackCompassRole, 0, state.backCompass)
                chunk.setData(SurveyChunk.ShotClinoRole, 0, state.clino)
                chunk.setData(SurveyChunk.ShotBackClinoRole, 0, state.backClino)
                chunk.setData(SurveyChunk.StationLeftRole, 0, state.left)
                chunk.setData(SurveyChunk.StationRightRole, 0, state.right)
                chunk.setData(SurveyChunk.StationUpRole, 0, state.up)
                chunk.setData(SurveyChunk.StationDownRole, 0, state.down)
            }

            let nextChunkState = function(baselineStateJson) {
                let baseline = JSON.parse(baselineStateJson)
                return JSON.stringify({
                    backSightsEnabled: true,
                    stationName: baseline.stationName === "TS-01" ? "TS-02" : "TS-01",
                    distance: baseline.distance === "12.34" ? "10.01" : "12.34",
                    compass: baseline.compass === "123.45" ? "45.67" : "123.45",
                    backCompass: baseline.backCompass === "321.54" ? "210.98" : "321.54",
                    clino: baseline.clino === "-12.30" ? "5.60" : "-12.30",
                    backClino: baseline.backClino === "11.70" ? "-4.20" : "11.70",
                    left: baseline.left === "1.10" ? "0.50" : "1.10",
                    right: baseline.right === "2.20" ? "0.80" : "2.20",
                    up: baseline.up === "3.30" ? "1.20" : "3.30",
                    down: baseline.down === "4.40" ? "1.60" : "4.40"
                })
            }

            let surveyDataCell = function(row, role) {
                let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
                verify(view !== null)
                view.positionViewAtIndex(row, ListView.Contain)
                // waitForRendering(rootId)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + role)
                verify(cell !== null)
                return cell
            }

            let surveyDataCellText = function(row, role) {
                let cell = surveyDataCell(row, role)
                let coreTextInput = findDescendantByObjectName(cell, "coreTextInput")
                if (coreTextInput !== null && coreTextInput.text !== undefined) {
                    return String(coreTextInput.text)
                }
                if (cell.text !== undefined) {
                    return String(cell.text)
                }
                let textNode = findDescendantWhere(cell, function(item) {
                    return item !== null
                           && item !== undefined
                           && item.text !== undefined
                })
                if (textNode !== null) {
                    return String(textNode.text)
                }
                return ""
            }

            let snapshotChunkUiState = function() {
                let backSightsCheck = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->backSightCalibrationEditor->checkBox")
                verify(backSightsCheck !== null)
                let uiState = JSON.stringify({
                    backSightsEnabled: backSightsCheck.checked === true,
                    stationName: surveyDataCellText(3, SurveyChunk.StationNameRole),
                    distance: surveyDataCellText(2, SurveyChunk.ShotDistanceRole),
                    compass: surveyDataCellText(2, SurveyChunk.ShotCompassRole),
                    backCompass: surveyDataCellText(2, SurveyChunk.ShotBackCompassRole),
                    clino: surveyDataCellText(2, SurveyChunk.ShotClinoRole),
                    backClino: surveyDataCellText(2, SurveyChunk.ShotBackClinoRole),
                    left: surveyDataCellText(1, SurveyChunk.StationLeftRole),
                    right: surveyDataCellText(1, SurveyChunk.StationRightRole),
                    up: surveyDataCellText(1, SurveyChunk.StationUpRole),
                    down: surveyDataCellText(1, SurveyChunk.StationDownRole)
                })
                return uiState
            }

            runTripSyncRoundTrip(
                context.tripPageAddress,
                function() {
                    return snapshotChunkState()
                },
                function(stateJson) {
                    applyChunkState(stateJson)
                },
                function(baselineStateJson) {
                    return nextChunkState(baselineStateJson)
                },
                function() {
                    return snapshotChunkUiState()
                }
            )
        }

        function test_tripChunkInsertShotSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let currentTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let currentChunk = function() {
                let trip = currentTrip()
                verify(trip.chunkCount > 0)
                let chunk = trip.chunk(0)
                verify(chunk !== null)
                verify(chunk.stationCount >= 2)
                verify(chunk.shotCount >= 1)
                return chunk
            }

            let readingText = function(value) {
                if (value !== null && value !== undefined && value.value !== undefined) {
                    return String(value.value)
                }
                return String(value)
            }

            let rowForStationIndex = function(stationIndex) {
                return 1 + (stationIndex * 2)
            }

            let rowForShotIndex = function(shotIndex) {
                return 2 + (shotIndex * 2)
            }

            let surveyDataCell = function(row, role) {
                let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
                verify(view !== null)
                view.positionViewAtIndex(row, ListView.Contain)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + role)
                verify(cell !== null)
                return cell
            }

            let surveyDataCellText = function(row, role) {
                let cell = surveyDataCell(row, role)
                let coreTextInput = findDescendantByObjectName(cell, "coreTextInput")
                if (coreTextInput !== null && coreTextInput.text !== undefined) {
                    return String(coreTextInput.text)
                }
                if (cell.text !== undefined) {
                    return String(cell.text)
                }
                let textNode = findDescendantWhere(cell, function(item) {
                    return item !== null
                           && item !== undefined
                           && item.text !== undefined
                })
                if (textNode !== null) {
                    return String(textNode.text)
                }
                return ""
            }

            let snapshotInsertedTailState = function() {
                let trip = currentTrip()
                let calibration = trip.calibration
                let chunk = currentChunk()
                let tailShotIndex = chunk.shotCount - 1
                let tailStationIndex = chunk.stationCount - 1
                return JSON.stringify({
                    backSightsEnabled: calibration.backSights === true,
                    shotCount: chunk.shotCount,
                    stationCount: chunk.stationCount,
                    stationName: String(chunk.data(SurveyChunk.StationNameRole, tailStationIndex)),
                    distance: readingText(chunk.data(SurveyChunk.ShotDistanceRole, tailShotIndex)),
                    compass: readingText(chunk.data(SurveyChunk.ShotCompassRole, tailShotIndex)),
                    backCompass: readingText(chunk.data(SurveyChunk.ShotBackCompassRole, tailShotIndex)),
                    clino: readingText(chunk.data(SurveyChunk.ShotClinoRole, tailShotIndex)),
                    backClino: readingText(chunk.data(SurveyChunk.ShotBackClinoRole, tailShotIndex)),
                    left: readingText(chunk.data(SurveyChunk.StationLeftRole, tailStationIndex)),
                    right: readingText(chunk.data(SurveyChunk.StationRightRole, tailStationIndex)),
                    up: readingText(chunk.data(SurveyChunk.StationUpRole, tailStationIndex)),
                    down: readingText(chunk.data(SurveyChunk.StationDownRole, tailStationIndex))
                })
            }

            let applyInsertedTailState = function(stateJson) {
                let state = JSON.parse(stateJson)
                let trip = currentTrip()
                let calibration = trip.calibration
                let chunk = currentChunk()

                calibration.backSights = state.backSightsEnabled

                let expectedShotCount = Number(state.shotCount)
                while (chunk.shotCount < expectedShotCount) {
                    chunk.insertShot(chunk.shotCount - 1, SurveyChunk.Below)
                }

                let tailShotIndex = chunk.shotCount - 1
                let tailStationIndex = chunk.stationCount - 1

                chunk.setData(SurveyChunk.StationNameRole, tailStationIndex, state.stationName)
                chunk.setData(SurveyChunk.ShotDistanceRole, tailShotIndex, state.distance)
                chunk.setData(SurveyChunk.ShotCompassRole, tailShotIndex, state.compass)
                chunk.setData(SurveyChunk.ShotBackCompassRole, tailShotIndex, state.backCompass)
                chunk.setData(SurveyChunk.ShotClinoRole, tailShotIndex, state.clino)
                chunk.setData(SurveyChunk.ShotBackClinoRole, tailShotIndex, state.backClino)
                chunk.setData(SurveyChunk.StationLeftRole, tailStationIndex, state.left)
                chunk.setData(SurveyChunk.StationRightRole, tailStationIndex, state.right)
                chunk.setData(SurveyChunk.StationUpRole, tailStationIndex, state.up)
                chunk.setData(SurveyChunk.StationDownRole, tailStationIndex, state.down)
            }

            let nextInsertedTailState = function(baselineStateJson) {
                let baseline = JSON.parse(baselineStateJson)
                return JSON.stringify({
                    backSightsEnabled: true,
                    shotCount: Number(baseline.shotCount) + 1,
                    stationCount: Number(baseline.stationCount) + 1,
                    stationName: "SYNC-ADD-1",
                    distance: "9.87",
                    compass: "111.10",
                    backCompass: "291.10",
                    clino: "-6.50",
                    backClino: "6.50",
                    left: "1.20",
                    right: "2.30",
                    up: "3.40",
                    down: "4.50"
                })
            }

            let snapshotInsertedTailUiState = function() {
                let trip = currentTrip()
                let chunk = currentChunk()
                let tailShotIndex = chunk.shotCount - 1
                let tailStationIndex = chunk.stationCount - 1
                let backSightsCheck = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->backSightCalibrationEditor->checkBox")
                verify(backSightsCheck !== null)
                return JSON.stringify({
                    backSightsEnabled: backSightsCheck.checked === true,
                    shotCount: chunk.shotCount,
                    stationCount: chunk.stationCount,
                    stationName: surveyDataCellText(rowForStationIndex(tailStationIndex), SurveyChunk.StationNameRole),
                    distance: surveyDataCellText(rowForShotIndex(tailShotIndex), SurveyChunk.ShotDistanceRole),
                    compass: surveyDataCellText(rowForShotIndex(tailShotIndex), SurveyChunk.ShotCompassRole),
                    backCompass: surveyDataCellText(rowForShotIndex(tailShotIndex), SurveyChunk.ShotBackCompassRole),
                    clino: surveyDataCellText(rowForShotIndex(tailShotIndex), SurveyChunk.ShotClinoRole),
                    backClino: surveyDataCellText(rowForShotIndex(tailShotIndex), SurveyChunk.ShotBackClinoRole),
                    left: surveyDataCellText(rowForStationIndex(tailStationIndex), SurveyChunk.StationLeftRole),
                    right: surveyDataCellText(rowForStationIndex(tailStationIndex), SurveyChunk.StationRightRole),
                    up: surveyDataCellText(rowForStationIndex(tailStationIndex), SurveyChunk.StationUpRole),
                    down: surveyDataCellText(rowForStationIndex(tailStationIndex), SurveyChunk.StationDownRole)
                })
            }

            runTripSyncRoundTrip(
                context.tripPageAddress,
                function() {
                    return snapshotInsertedTailState()
                },
                function(stateJson) {
                    applyInsertedTailState(stateJson)
                },
                function(baselineStateJson) {
                    return nextInsertedTailState(baselineStateJson)
                },
                function() {
                    return snapshotInsertedTailUiState()
                }
            )
        }

        function test_tripChunkRemoveShotSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let currentTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let currentChunk = function() {
                let trip = currentTrip()
                verify(trip.chunkCount > 0)
                let chunk = trip.chunk(0)
                verify(chunk !== null)
                verify(chunk.stationCount >= 2)
                verify(chunk.shotCount >= 1)
                return chunk
            }

            let readingText = function(value) {
                if (value !== null && value !== undefined && value.value !== undefined) {
                    return String(value.value)
                }
                return String(value)
            }

            let canonicalNumericText = function(value) {
                let stringValue = String(value)
                let numberValue = Number(stringValue)
                if (!Number.isFinite(numberValue)) {
                    return stringValue
                }
                return String(numberValue)
            }

            let rowForStationIndex = function(stationIndex) {
                return 1 + (stationIndex * 2)
            }

            let rowForShotIndex = function(shotIndex) {
                return 2 + (shotIndex * 2)
            }

            let surveyDataCell = function(row, role) {
                let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
                verify(view !== null)
                view.positionViewAtIndex(row, ListView.Contain)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + role)
                verify(cell !== null)
                return cell
            }

            let surveyDataCellText = function(row, role) {
                let cell = surveyDataCell(row, role)
                let coreTextInput = findDescendantByObjectName(cell, "coreTextInput")
                if (coreTextInput !== null && coreTextInput.text !== undefined) {
                    return String(coreTextInput.text)
                }
                if (cell.text !== undefined) {
                    return String(cell.text)
                }
                let textNode = findDescendantWhere(cell, function(item) {
                    return item !== null
                           && item !== undefined
                           && item.text !== undefined
                })
                if (textNode !== null) {
                    return String(textNode.text)
                }
                return ""
            }

            let containsStationName = function(chunk, stationName) {
                for (let i = 0; i < chunk.stationCount; ++i) {
                    if (String(chunk.data(SurveyChunk.StationNameRole, i)) === stationName) {
                        return true
                    }
                }
                return false
            }

            let modelChunkSummary = function(chunk) {
                let stationNames = []
                for (let i = 0; i < chunk.stationCount; ++i) {
                    stationNames.push(String(chunk.data(SurveyChunk.StationNameRole, i)))
                }

                let shotSummary = []
                for (let i = 0; i < chunk.shotCount; ++i) {
                    shotSummary.push(String(chunk.data(SurveyChunk.ShotDistanceRole, i)) + "/"
                                     + String(chunk.data(SurveyChunk.ShotCompassRole, i)) + "/"
                                     + String(chunk.data(SurveyChunk.ShotClinoRole, i)))
                }

                return "stations=[" + stationNames.join(", ") + "] shots=[" + shotSummary.join(", ") + "]"
            }

            let verifyTailUiMatchesModel = function(chunk) {
                let tailShotIndex = chunk.shotCount - 1
                let tailStationIndex = chunk.stationCount - 1
                compare(canonicalNumericText(surveyDataCellText(rowForShotIndex(tailShotIndex), SurveyChunk.ShotDistanceRole)),
                        canonicalNumericText(readingText(chunk.data(SurveyChunk.ShotDistanceRole, tailShotIndex))))
                compare(canonicalNumericText(surveyDataCellText(rowForShotIndex(tailShotIndex), SurveyChunk.ShotCompassRole)),
                        canonicalNumericText(readingText(chunk.data(SurveyChunk.ShotCompassRole, tailShotIndex))))
                compare(canonicalNumericText(surveyDataCellText(rowForShotIndex(tailShotIndex), SurveyChunk.ShotClinoRole)),
                        canonicalNumericText(readingText(chunk.data(SurveyChunk.ShotClinoRole, tailShotIndex))))
                compare(surveyDataCellText(rowForStationIndex(tailStationIndex), SurveyChunk.StationNameRole),
                        String(chunk.data(SurveyChunk.StationNameRole, tailStationIndex)))
            }

            let trip = currentTrip()
            let chunk = currentChunk()
            let baselineShotCount = chunk.shotCount
            let baselineStationCount = chunk.stationCount
            let removedStationName = String(chunk.data(SurveyChunk.StationNameRole, chunk.stationCount - 1))
            verify(removedStationName.length > 0)
            console.log("[TripSyncQmlDebug] remove-shot baseline summary=", modelChunkSummary(chunk))

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            trip.calibration.backSights = true
            chunk.removeShot(chunk.shotCount - 1, SurveyChunk.Below)

            tryVerifyWithDiagnostics(() => {
                let updatedChunk = currentChunk()
                return updatedChunk.shotCount === baselineShotCount - 1
                       && updatedChunk.stationCount === baselineStationCount - 1
            }, 3000, "verify removal after local setter")
            console.log("[TripSyncQmlDebug] remove-shot after local remove summary=", modelChunkSummary(currentChunk()))

            let backSightsCheck = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->backSightCalibrationEditor->checkBox")
            verify(backSightsCheck !== null)
            compare(backSightsCheck.checked, true)
            verifyTailUiMatchesModel(currentChunk())

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")
            verifyStillOnTripPage(context.tripPageAddress)

            tryVerifyWithDiagnostics(() => {
                let restoredChunk = currentChunk()
                return restoredChunk.shotCount === baselineShotCount
                       && restoredChunk.stationCount === baselineStationCount
            }, 3000, "verify baseline after checkout", function(attempts, elapsedMs) {
                console.log("[TripSyncQmlDebug] remove-shot pending baseline-after-checkout attempts=" + attempts
                            + " elapsedMs=" + elapsedMs
                            + " summary=" + modelChunkSummary(currentChunk()))
            })
            console.log("[TripSyncQmlDebug] remove-shot after checkout summary=", modelChunkSummary(currentChunk()))

            verifyTailUiMatchesModel(currentChunk())

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitB === commitC)
            verifyStillOnTripPage(context.tripPageAddress)

            tryVerifyWithDiagnostics(() => {
                let resyncedChunk = currentChunk()
                return resyncedChunk.shotCount === baselineShotCount - 1
                       && resyncedChunk.stationCount === baselineStationCount - 1
            }, 3000, "verify removal after second sync", function(attempts, elapsedMs) {
                let pendingChunk = currentChunk()
                console.log("[TripSyncQmlDebug] remove-shot pending second-sync attempts=" + attempts
                            + " elapsedMs=" + elapsedMs
                            + " shotCount=" + pendingChunk.shotCount
                            + " stationCount=" + pendingChunk.stationCount
                            + " containsRemovedStation=" + containsStationName(pendingChunk, removedStationName)
                            + " summary=" + modelChunkSummary(pendingChunk))
            })
            console.log("[TripSyncQmlDebug] remove-shot after second sync summary=", modelChunkSummary(currentChunk()))

            compare(backSightsCheck.checked, true)
            verifyTailUiMatchesModel(currentChunk())
        }

        function test_tripChunkRemoveAboveSelectedTailStationSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
            verify(view !== null)
            let editorModel = view.model
            verify(editorModel !== null)

            let currentTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let currentChunk = function() {
                let trip = currentTrip()
                verify(trip.chunkCount > 0)
                let chunk = trip.chunk(0)
                verify(chunk !== null)
                verify(chunk.stationCount >= 3)
                verify(chunk.shotCount >= 2)
                return chunk
            }

            let readingText = function(value) {
                if (value !== null && value !== undefined && value.value !== undefined) {
                    return String(value.value)
                }
                return String(value)
            }

            let rowForStationIndex = function(chunk, stationIndex) {
                return editorModel.toModelRow(
                            editorModel.rowIndex(chunk, stationIndex, SurveyEditorRowIndex.StationRow))
            }

            let rowForShotIndex = function(chunk, shotIndex) {
                return editorModel.toModelRow(
                            editorModel.rowIndex(chunk, shotIndex, SurveyEditorRowIndex.ShotRow))
            }

            let surveyDataCell = function(row, role) {
                let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
                verify(view !== null)
                view.positionViewAtIndex(row, ListView.Contain)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + role)
                verify(cell !== null)
                return cell
            }

            let surveyDataCellText = function(row, role) {
                let cell = surveyDataCell(row, role)
                let coreTextInput = findDescendantByObjectName(cell, "coreTextInput")
                if (coreTextInput !== null && coreTextInput.text !== undefined) {
                    return String(coreTextInput.text)
                }
                if (cell.text !== undefined) {
                    return String(cell.text)
                }
                let textNode = findDescendantWhere(cell, function(item) {
                    return item !== null
                           && item !== undefined
                           && item.text !== undefined
                })
                if (textNode !== null) {
                    return String(textNode.text)
                }
                return ""
            }

            let visibleStationCount = function(chunk) {
                let count = 0
                while (rowForStationIndex(chunk, count) >= 0) {
                    count += 1
                }
                return count
            }

            let visibleShotCount = function(chunk) {
                let count = 0
                while (rowForShotIndex(chunk, count) >= 0) {
                    count += 1
                }
                return count
            }

            let stationNameAt = function(chunk, stationIndex) {
                let row = editorModel.toModelRow(editorModel.rowIndex(
                                                     chunk,
                                                     stationIndex,
                                                     SurveyEditorRowIndex.StationRow))
                let stationData = editorModel.data(editorModel.index(row, 0),
                                                   SurveyEditorModel.StationNameRole)
                return String(stationData.reading.value)
            }

            let shotReadingAt = function(chunk, shotIndex, role) {
                let row = editorModel.toModelRow(editorModel.rowIndex(
                                                     chunk,
                                                     shotIndex,
                                                     SurveyEditorRowIndex.ShotRow))
                let modelRole = SurveyEditorModel.ShotDistanceRole
                switch (role) {
                case SurveyChunk.ShotDistanceRole:
                    modelRole = SurveyEditorModel.ShotDistanceRole
                    break
                case SurveyChunk.ShotCompassRole:
                    modelRole = SurveyEditorModel.ShotCompassRole
                    break
                case SurveyChunk.ShotBackCompassRole:
                    modelRole = SurveyEditorModel.ShotBackCompassRole
                    break
                case SurveyChunk.ShotClinoRole:
                    modelRole = SurveyEditorModel.ShotClinoRole
                    break
                case SurveyChunk.ShotBackClinoRole:
                    modelRole = SurveyEditorModel.ShotBackClinoRole
                    break
                }
                let shotData = editorModel.data(editorModel.index(row, 0), modelRole)
                return shotData.reading
            }

            let chunkSummary = function(chunk) {
                let stationCount = visibleStationCount(chunk)
                let shotCount = visibleShotCount(chunk)
                let stationNames = []
                for (let i = 0; i < stationCount; ++i) {
                    stationNames.push(stationNameAt(chunk, i))
                }

                let shotValues = []
                for (let i = 0; i < shotCount; ++i) {
                    shotValues.push({
                        distance: readingText(shotReadingAt(chunk, i, SurveyChunk.ShotDistanceRole)),
                        compass: readingText(shotReadingAt(chunk, i, SurveyChunk.ShotCompassRole)),
                        backCompass: readingText(shotReadingAt(chunk, i, SurveyChunk.ShotBackCompassRole)),
                        clino: readingText(shotReadingAt(chunk, i, SurveyChunk.ShotClinoRole)),
                        backClino: readingText(shotReadingAt(chunk, i, SurveyChunk.ShotBackClinoRole))
                    })
                }

                while (stationNames.length > 0 && shotValues.length > 0) {
                    let lastStationEmpty = String(stationNames[stationNames.length - 1]).trim().length === 0
                    let lastShot = shotValues[shotValues.length - 1]
                    let lastShotEmpty = String(lastShot.distance).trim().length === 0
                            && String(lastShot.compass).trim().length === 0
                            && String(lastShot.backCompass).trim().length === 0
                            && String(lastShot.clino).trim().length === 0
                            && String(lastShot.backClino).trim().length === 0
                    if (!(lastStationEmpty && lastShotEmpty)) {
                        break
                    }
                    stationNames.pop()
                    shotValues.pop()
                }

                return JSON.stringify({
                    stationCount: stationNames.length,
                    shotCount: shotValues.length,
                    stationNames: stationNames,
                    shots: shotValues
                })
            }

            let containsStationName = function(chunk, stationName) {
                let stationCount = visibleStationCount(chunk)
                for (let i = 0; i < stationCount; ++i) {
                    if (stationNameAt(chunk, i) === stationName) {
                        return true
                    }
                }
                return false
            }

            let findLastNamedStationIndex = function(chunk) {
                let stationCount = visibleStationCount(chunk)
                for (let i = stationCount - 1; i >= 0; --i) {
                    let stationName = stationNameAt(chunk, i).trim()
                    if (stationName.length > 0) {
                        return i
                    }
                }
                return -1
            }

            let trip = currentTrip()
            trip.calibration.backSights = true

            let chunk = currentChunk()
            let baselineSummary = chunkSummary(chunk)
            let baselineShotCount = visibleShotCount(chunk)
            let baselineStationCount = visibleStationCount(chunk)
            let baselineLastNamedStationIndex = findLastNamedStationIndex(chunk)
            verify(baselineLastNamedStationIndex >= 1)
            verify(baselineLastNamedStationIndex < baselineStationCount)

            let hadTrailingEmptyStationBefore = stationNameAt(chunk, baselineStationCount - 1).trim().length === 0
            let expectedShotCountAfterSelection = hadTrailingEmptyStationBefore ? baselineShotCount : baselineShotCount + 1
            let expectedStationCountAfterSelection = hadTrailingEmptyStationBefore ? baselineStationCount : baselineStationCount + 1
            let baselineModifiedFileCount = TestHelper.projectModifiedFileCount(RootData.project)

            let selectedStationCell = surveyDataCell(rowForStationIndex(chunk, baselineLastNamedStationIndex), SurveyChunk.StationNameRole)
            mouseClick(selectedStationCell, 8, selectedStationCell.height * 0.5, Qt.LeftButton)

            tryVerifyWithDiagnostics(() => {
                let selectedChunk = currentChunk()
                if (visibleShotCount(selectedChunk) !== expectedShotCountAfterSelection
                        || visibleStationCount(selectedChunk) !== expectedStationCountAfterSelection) {
                    return false
                }

                let lastStationName = stationNameAt(selectedChunk, visibleStationCount(selectedChunk) - 1).trim()
                return lastStationName.length === 0
            }, 3000, "verify selecting last named station appends transient empty tail row")

            tryVerifyWithDiagnostics(() => {
                return TestHelper.projectModifiedFileCount(RootData.project) === baselineModifiedFileCount
            }, 3000, "verify transient tail row does not dirty working tree")

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            let removalChunkBefore = currentChunk()
            let selectedStationIndexBeforeRemoval = findLastNamedStationIndex(removalChunkBefore)
            verify(selectedStationIndexBeforeRemoval >= 1)
            let removedStationName = stationNameAt(removalChunkBefore, selectedStationIndexBeforeRemoval - 1)
            verify(removedStationName.trim().length > 0)
            removalChunkBefore.removeShot(selectedStationIndexBeforeRemoval - 1, SurveyChunk.Above)

            tryVerifyWithDiagnostics(() => {
                let updatedChunk = currentChunk()
                return !containsStationName(updatedChunk, removedStationName)
            }, 3000, "verify station above selected station removed")

            let removedSummary = chunkSummary(currentChunk())
            verify(removedSummary !== baselineSummary)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")
            verifyStillOnTripPage(context.tripPageAddress)

            tryVerifyWithDiagnostics(() => {
                return chunkSummary(currentChunk()) === baselineSummary
            }, 3000, "verify baseline chunk state after checkout")

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitC === commitB)
            verifyStillOnTripPage(context.tripPageAddress)

            tryVerifyWithDiagnostics(() => {
                return chunkSummary(currentChunk()) === removedSummary
            }, 3000, "verify removed chunk state after second sync")

            let finalChunk = currentChunk()
            let finalTailStationIndex = visibleStationCount(finalChunk) - 1
            let finalTailShotIndex = visibleShotCount(finalChunk) - 1
            compare(surveyDataCellText(rowForStationIndex(finalChunk, finalTailStationIndex), SurveyChunk.StationNameRole),
                    stationNameAt(finalChunk, finalTailStationIndex))
            compare(surveyDataCellText(rowForShotIndex(finalChunk, finalTailShotIndex), SurveyChunk.ShotDistanceRole),
                    readingText(shotReadingAt(finalChunk, finalTailShotIndex, SurveyChunk.ShotDistanceRole)))
        }

        function test_tripRemoveChunkFromStationContextMenuSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
            verify(view !== null)
            let editorModel = view.model
            verify(editorModel !== null)

            let currentTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let rowForStationIndex = function(chunk, stationIndex) {
                return editorModel.toModelRow(
                            editorModel.rowIndex(chunk, stationIndex, SurveyEditorRowIndex.StationRow))
            }

            let surveyDataCell = function(row, role) {
                view.positionViewAtIndex(row, ListView.Contain)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + role)
                verify(cell !== null)
                return cell
            }

            let openContextMenu = function(row, role) {
                let cell = surveyDataCell(row, role)
                mouseClick(cell, cell.width * 0.5, cell.height * 0.5, Qt.RightButton)
                waitForRendering(rootId)
            }

            let triggerMenuItemFromLoader = function(row, role, loaderObjectName, itemObjectName) {
                let loaderPath = "rootId->tripPage->view->dataBox." + row + "." + role + "->" + loaderObjectName
                let loader = ObjectFinder.findObjectByChain(mainWindow, loaderPath)
                verify(loader !== null)
                verify(loader.item !== null)
                let item = findChild(loader.item, itemObjectName)
                verify(item !== null)
                item.triggered()
                waitForRendering(rootId)
            }

            let snapshotTripState = function() {
                let trip = currentTrip()
                let chunks = []
                for (let c = 0; c < trip.chunkCount; ++c) {
                    let chunk = trip.chunk(c)
                    verify(chunk !== null)

                    chunks.push({
                        firstStationName: chunk.stationCount > 0
                                          ? String(chunk.data(SurveyChunk.StationNameRole, 0))
                                          : "",
                        stationCount: chunk.stationCount,
                        shotCount: chunk.shotCount
                    })
                }

                return {
                    chunkCount: trip.chunkCount,
                    chunks: chunks
                }
            }

            let trip = currentTrip()
            if (trip.chunkCount < 2) {
                trip.addNewChunk()
                tryVerifyWithDiagnostics(() => {
                    return currentTrip().chunkCount >= 2
                }, 3000, "ensure second chunk exists before removal")

                verify(RootData.project.sync())
                waitForProjectSyncToFinish()
                verifyStillOnTripPage(context.tripPageAddress)
            }

            let baselineState = snapshotTripState()
            verify(baselineState.chunkCount >= 2)
            let baselineSummary = JSON.stringify(baselineState)
            verify(baselineSummary.length > 0)

            let targetChunkIndex = baselineState.chunkCount - 1
            let expectedRemovedState = JSON.parse(baselineSummary)
            expectedRemovedState.chunks.splice(targetChunkIndex, 1)
            expectedRemovedState.chunkCount -= 1
            let expectedRemovedSummary = JSON.stringify(expectedRemovedState)

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            let targetChunk = currentTrip().chunk(targetChunkIndex)
            verify(targetChunk !== null)
            let targetRow = rowForStationIndex(targetChunk, 0)
            verify(targetRow >= 0)

            openContextMenu(targetRow, SurveyChunk.StationNameRole)
            triggerMenuItemFromLoader(targetRow,
                                      SurveyChunk.StationNameRole,
                                      "stationMenuLoader",
                                      "stationMenuRemoveChunk")

            tryVerifyWithDiagnostics(() => {
                return JSON.stringify(snapshotTripState()) === expectedRemovedSummary
            }, 3000, "verify chunk removal after local context-menu action")

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")
            verifyStillOnTripPage(context.tripPageAddress)

            tryVerifyWithDiagnostics(() => {
                return JSON.stringify(snapshotTripState()) === baselineSummary
            }, 3000, "verify baseline state after checkout")

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitC === commitB)
            verifyStillOnTripPage(context.tripPageAddress)

            tryVerifyWithDiagnostics(() => {
                return JSON.stringify(snapshotTripState()) === expectedRemovedSummary
            }, 3000, "verify removed state after second sync")
        }

        function test_tripAddEmptyChunkFromAddAnotherDataBlockSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let currentTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let snapshotTripState = function() {
                let trip = currentTrip()
                let lastChunk = trip.chunkCount > 0 ? trip.chunk(trip.chunkCount - 1) : null
                return JSON.stringify({
                    chunkCount: trip.chunkCount,
                    lastChunk: lastChunk === null
                               ? null
                               : {
                                     stationCount: lastChunk.stationCount,
                                     shotCount: lastChunk.shotCount
                                 }
                })
            }

            runTripSyncRoundTrip(
                context.tripPageAddress,
                function() {
                    return snapshotTripState()
                },
                function(expectedStateJson) {
                    let addDataBlockButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->spaceAddBar")
                    verify(addDataBlockButton !== null)
                    addDataBlockButton.clicked()
                    waitForRendering(rootId)

                    tryVerifyWithDiagnostics(() => {
                        return snapshotTripState() === expectedStateJson
                    }, 3000, "verify new empty chunk after add another data block")
                },
                function(baselineStateJson) {
                    let nextState = JSON.parse(baselineStateJson)
                    nextState.chunkCount += 1
                    nextState.lastChunk = {
                        stationCount: 2,
                        shotCount: 1
                    }
                    return JSON.stringify(nextState)
                }
            )
        }

        function test_tripAddFilledChunkFromSpacebarSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
            verify(view !== null)
            let editorModel = view.model
            verify(editorModel !== null)

            let currentTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let readingText = function(value) {
                if (value !== null && value !== undefined && value.value !== undefined) {
                    return String(value.value)
                }
                return String(value)
            }

            let snapshotTripState = function() {
                let trip = currentTrip()
                let lastChunk = trip.chunkCount > 0 ? trip.chunk(trip.chunkCount - 1) : null
                return JSON.stringify({
                    chunkCount: trip.chunkCount,
                    lastChunk: lastChunk === null
                               ? null
                               : {
                                     stationCount: lastChunk.stationCount,
                                     shotCount: lastChunk.shotCount,
                                     station0: String(lastChunk.data(SurveyChunk.StationNameRole, 0)),
                                     station1: String(lastChunk.data(SurveyChunk.StationNameRole, 1)),
                                     distance0: readingText(lastChunk.data(SurveyChunk.ShotDistanceRole, 0)),
                                     compass0: readingText(lastChunk.data(SurveyChunk.ShotCompassRole, 0)),
                                     clino0: readingText(lastChunk.data(SurveyChunk.ShotClinoRole, 0))
                                 }
                })
            }

            let rowForStationIndex = function(chunk, stationIndex) {
                return editorModel.toModelRow(
                            editorModel.rowIndex(chunk, stationIndex, SurveyEditorRowIndex.StationRow))
            }

            let surveyDataCell = function(row, role) {
                view.positionViewAtIndex(row, ListView.Contain)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + role)
                verify(cell !== null)
                return cell
            }

            runTripSyncRoundTrip(
                context.tripPageAddress,
                function() {
                    return snapshotTripState()
                },
                function(expectedStateJson) {
                    let tripBeforeAdd = currentTrip()
                    let baselineChunkCountBeforeAdd = tripBeforeAdd.chunkCount
                    let sourceChunk = tripBeforeAdd.chunk(tripBeforeAdd.chunkCount - 1)
                    verify(sourceChunk !== null)
                    if (sourceChunk.isStationAndShotsEmpty()) {
                        sourceChunk.setData(SurveyChunk.StationNameRole, 0, "PRE-SPACE-1")
                        sourceChunk.setData(SurveyChunk.StationNameRole, 1, "PRE-SPACE-2")
                        sourceChunk.setData(SurveyChunk.ShotDistanceRole, 0, "1")
                        sourceChunk.setData(SurveyChunk.ShotCompassRole, 0, "10")
                        sourceChunk.setData(SurveyChunk.ShotClinoRole, 0, "0")
                        tryVerifyWithDiagnostics(() => {
                            return sourceChunk.isStationAndShotsEmpty() === false
                        }, 3000, "ensure last chunk is non-empty before spacebar add")
                    }
                    let sourceRow = rowForStationIndex(sourceChunk, sourceChunk.stationCount - 1)
                    verify(sourceRow >= 0)
                    let sourceCell = surveyDataCell(sourceRow, SurveyChunk.StationNameRole)
                    sourceCell.addNewChunk()
                    waitForRendering(rootId)

                    tryVerifyWithDiagnostics(() => {
                        return currentTrip().chunkCount === baselineChunkCountBeforeAdd + 1
                    }, 3000, "verify chunk added via spacebar")

                    let expectedState = JSON.parse(expectedStateJson)
                    let expectedChunk = expectedState.lastChunk
                    let newChunk = currentTrip().chunk(currentTrip().chunkCount - 1)
                    verify(newChunk !== null)
                    compare(newChunk.stationCount, 2)
                    compare(newChunk.shotCount, 1)

                    newChunk.setData(SurveyChunk.StationNameRole, 0, expectedChunk.station0)
                    newChunk.setData(SurveyChunk.StationNameRole, 1, expectedChunk.station1)
                    newChunk.setData(SurveyChunk.ShotDistanceRole, 0, expectedChunk.distance0)
                    newChunk.setData(SurveyChunk.ShotCompassRole, 0, expectedChunk.compass0)
                    newChunk.setData(SurveyChunk.ShotClinoRole, 0, expectedChunk.clino0)

                    tryVerifyWithDiagnostics(() => {
                        return snapshotTripState() === expectedStateJson
                    }, 3000, "verify new filled chunk after spacebar add")
                },
                function(baselineStateJson) {
                    let nextState = JSON.parse(baselineStateJson)
                    nextState.chunkCount += 1
                    nextState.lastChunk = {
                        stationCount: 2,
                        shotCount: 1,
                        station0: "SYNC-NEW-A1",
                        station1: "SYNC-NEW-A2",
                        distance0: "5.67",
                        compass0: "123.40",
                        clino0: "-8.90"
                    }
                    return JSON.stringify(nextState)
                }
            )
        }

        function test_tripAddImageNoteSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let currentTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let notesModel = function() {
                let model = currentTrip().notes
                verify(model !== null)
                return model
            }

            let noteGalleryView = function() {
                let galleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView")
                verify(galleryView !== null)
                return galleryView
            }

            let snapshotModelState = function() {
                return JSON.stringify({
                    noteCount: notesModel().rowCount()
                })
            }

            let snapshotUiState = function() {
                return JSON.stringify({
                    noteCount: noteGalleryView().count
                })
            }

            let caveName = String(currentTrip().parentCave.name)
            let tripName = String(currentTrip().name)
            verify(caveName.length > 0)
            verify(tripName.length > 0)

            let baselineModelState = snapshotModelState()
            let baselineUiState = snapshotUiState()
            compare(baselineUiState, baselineModelState)

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            let editedState = JSON.parse(baselineModelState)
            editedState.noteCount += 1
            let expectedEditedState = JSON.stringify(editedState)

            let copiedImagePath = TestHelper.copyToTempDir("://datasets/test_cwTextureUploadTask/PhakeCave.PNG")
            verify(copiedImagePath !== "")
            let copiedImageUrl = Qt.url("file://" + copiedImagePath)

            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery")
            verify(noteGallery !== null)
            noteGallery.imagesAdded([copiedImageUrl])

            tryVerifyWithDiagnostics(() => {
                return snapshotModelState() === expectedEditedState
            }, 5000, "verify note added to model")
            tryVerifyWithDiagnostics(() => {
                return snapshotUiState() === expectedEditedState
            }, 5000, "verify note added to UI")

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")

            openTripPage(caveName, tripName)
            compare(snapshotModelState(), baselineModelState)
            compare(snapshotUiState(), baselineUiState)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitC === commitB)

            openTripPage(caveName, tripName)
            compare(snapshotModelState(), expectedEditedState)
            compare(snapshotUiState(), expectedEditedState)
        }

        function test_tripAddLiDARNoteSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let currentTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let imageNotesModel = function() {
                let model = currentTrip().notes
                verify(model !== null)
                return model
            }

            let lidarNotesModel = function() {
                let model = currentTrip().notesLiDAR
                verify(model !== null)
                return model
            }

            let noteGallery = function() {
                let gallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery")
                verify(gallery !== null)
                return gallery
            }

            let noteGalleryView = function() {
                let galleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView")
                verify(galleryView !== null)
                return galleryView
            }

            let noteLiDARViewer = function() {
                let viewer = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId")
                verify(viewer !== null)
                return viewer
            }

            let snapshotModelState = function() {
                return JSON.stringify({
                    imageNoteCount: imageNotesModel().rowCount(),
                    lidarNoteCount: lidarNotesModel().rowCount(),
                    totalNoteCount: imageNotesModel().rowCount() + lidarNotesModel().rowCount()
                })
            }

            let snapshotUiState = function() {
                return JSON.stringify({
                    totalNoteCount: noteGalleryView().count
                })
            }

            let verifySelectedLiDARNote = function(expectedFileName) {
                let galleryItem = ObjectFinder.findObjectByChain(
                    mainWindow,
                    "rootId->tripPage->noteGallery->galleryView->noteImage" + (noteGalleryView().count - 1))
                verify(galleryItem !== null)
                mouseClick(galleryItem)

                tryVerifyWithDiagnostics(() => {
                    return noteGallery().currentNoteLiDAR !== null
                           && String(noteGallery().currentNoteLiDAR.filename) === expectedFileName
                }, 10000, "verify selected LiDAR note")

                tryVerifyWithDiagnostics(() => {
                    return noteLiDARViewer().scene.gltf.status === RenderGLTF.Ready
                }, 10000, "verify LiDAR viewer ready")
            }

            let selectLastLiDARNote = function() {
                let galleryItem = ObjectFinder.findObjectByChain(
                    mainWindow,
                    "rootId->tripPage->noteGallery->galleryView->noteImage" + (noteGalleryView().count - 1))
                verify(galleryItem !== null)
                mouseClick(galleryItem)

                tryVerifyWithDiagnostics(() => {
                    return noteGallery().currentNoteLiDAR !== null
                }, 10000, "verify selected last LiDAR note")

                tryVerifyWithDiagnostics(() => {
                    return noteLiDARViewer().scene.gltf.status === RenderGLTF.Ready
                }, 10000, "verify selected last LiDAR viewer ready")

                return String(noteGallery().currentNoteLiDAR.filename)
            }

            let caveName = String(currentTrip().parentCave.name)
            let tripName = String(currentTrip().name)
            verify(caveName.length > 0)
            verify(tripName.length > 0)

            let baselineModelState = snapshotModelState()
            let baselineUiState = snapshotUiState()
            compare(snapshotUiState(), JSON.stringify({
                totalNoteCount: JSON.parse(baselineModelState).totalNoteCount
            }))

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            let copiedLiDARPath = TestHelper.copyToTempDir("://datasets/lidarProjects/9_15_2025 3.glb")
            verify(copiedLiDARPath !== "")
            let copiedLiDARUrl = Qt.url("file://" + copiedLiDARPath)
            let expectedFileName = "9_15_2025 3.glb"

            let editedState = JSON.parse(baselineModelState)
            editedState.lidarNoteCount += 1
            editedState.totalNoteCount += 1
            let expectedEditedModelState = JSON.stringify(editedState)
            let expectedEditedUiState = JSON.stringify({
                totalNoteCount: editedState.totalNoteCount
            })

            noteGallery().imagesAdded([copiedLiDARUrl])

            tryVerifyWithDiagnostics(() => {
                return snapshotModelState() === expectedEditedModelState
            }, 5000, "verify LiDAR note added to model")
            tryVerifyWithDiagnostics(() => {
                return snapshotUiState() === expectedEditedUiState
            }, 5000, "verify LiDAR note added to UI")
            verifySelectedLiDARNote(expectedFileName)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")

            openTripPage(caveName, tripName)
            compare(snapshotModelState(), baselineModelState)
            compare(snapshotUiState(), baselineUiState)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitC === commitB)

            openTripPage(caveName, tripName)
            compare(snapshotModelState(), expectedEditedModelState)
            compare(snapshotUiState(), expectedEditedUiState)
            verifySelectedLiDARNote(expectedFileName)
        }

        function test_tripRemoveLiDARNoteSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let currentTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let imageNotesModel = function() {
                let model = currentTrip().notes
                verify(model !== null)
                return model
            }

            let lidarNotesModel = function() {
                let model = currentTrip().notesLiDAR
                verify(model !== null)
                return model
            }

            let noteGallery = function() {
                let gallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery")
                verify(gallery !== null)
                return gallery
            }

            let noteGalleryView = function() {
                let galleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView")
                verify(galleryView !== null)
                return galleryView
            }

            let noteLiDARViewer = function() {
                let viewer = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId")
                verify(viewer !== null)
                return viewer
            }

            let snapshotModelState = function() {
                return JSON.stringify({
                    imageNoteCount: imageNotesModel().rowCount(),
                    lidarNoteCount: lidarNotesModel().rowCount(),
                    totalNoteCount: imageNotesModel().rowCount() + lidarNotesModel().rowCount()
                })
            }

            let snapshotUiState = function() {
                return JSON.stringify({
                    totalNoteCount: noteGalleryView().count
                })
            }

            let verifySelectedLiDARNote = function(expectedFileName) {
                let galleryItem = ObjectFinder.findObjectByChain(
                    mainWindow,
                    "rootId->tripPage->noteGallery->galleryView->noteImage" + (noteGalleryView().count - 1))
                verify(galleryItem !== null)
                mouseClick(galleryItem)

                tryVerifyWithDiagnostics(() => {
                    return noteGallery().currentNoteLiDAR !== null
                           && String(noteGallery().currentNoteLiDAR.filename) === expectedFileName
                }, 10000, "verify selected LiDAR note")

                tryVerifyWithDiagnostics(() => {
                    return noteLiDARViewer().scene.gltf.status === RenderGLTF.Ready
                }, 10000, "verify LiDAR viewer ready")
            }

            let selectLastLiDARNote = function() {
                let galleryItem = ObjectFinder.findObjectByChain(
                    mainWindow,
                    "rootId->tripPage->noteGallery->galleryView->noteImage" + (noteGalleryView().count - 1))
                verify(galleryItem !== null)
                mouseClick(galleryItem)

                tryVerifyWithDiagnostics(() => {
                    return noteGallery().currentNoteLiDAR !== null
                }, 10000, "verify selected last LiDAR note")

                tryVerifyWithDiagnostics(() => {
                    return noteLiDARViewer().scene.gltf.status === RenderGLTF.Ready
                }, 10000, "verify selected last LiDAR viewer ready")

                return String(noteGallery().currentNoteLiDAR.filename)
            }

            let ensureAtLeastOneLiDARNote = function() {
                if (lidarNotesModel().rowCount() > 0) {
                    return
                }

                let copiedLiDARPath = TestHelper.copyToTempDir("://datasets/lidarProjects/9_15_2025 3.glb")
                verify(copiedLiDARPath !== "")
                let copiedLiDARUrl = Qt.url("file://" + copiedLiDARPath)
                let expectedFileName = "9_15_2025 3.glb"

                noteGallery().imagesAdded([copiedLiDARUrl])

                tryVerifyWithDiagnostics(() => {
                    return lidarNotesModel().rowCount() > 0
                }, 5000, "seed at least one LiDAR note")
                tryVerifyWithDiagnostics(() => {
                    return noteGalleryView().count === imageNotesModel().rowCount() + lidarNotesModel().rowCount()
                }, 5000, "verify seeded LiDAR note in UI")
                verifySelectedLiDARNote(expectedFileName)

                verify(RootData.project.sync())
                waitForProjectSyncToFinish()
            }

            let caveName = String(currentTrip().parentCave.name)
            let tripName = String(currentTrip().name)
            verify(caveName.length > 0)
            verify(tripName.length > 0)

            ensureAtLeastOneLiDARNote()

            let baselineModelState = snapshotModelState()
            let baselineUiState = snapshotUiState()
            let baselineModel = JSON.parse(baselineModelState)
            let baselineUi = JSON.parse(baselineUiState)
            verify(Number(baselineModel.lidarNoteCount) > 0)

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            let removedLiDARAbsolutePath = ""
            let removedLiDARMetadataPath = ""
            let removedFileName = selectLastLiDARNote()
            verify(removedFileName.length > 0)
            removedLiDARAbsolutePath = String(RootData.project.absolutePath(noteGallery().currentNoteLiDAR))
            verify(removedLiDARAbsolutePath.length > 0)
            let lastSlashIndex = removedLiDARAbsolutePath.lastIndexOf("/")
            verify(lastSlashIndex >= 0)
            removedLiDARMetadataPath = removedLiDARAbsolutePath.slice(0, lastSlashIndex + 1) + removedFileName + ".cwnote3d"

            lidarNotesModel().removeNote(lidarNotesModel().rowCount() - 1)

            let expectedEditedModel = {
                imageNoteCount: Number(baselineModel.imageNoteCount),
                lidarNoteCount: Number(baselineModel.lidarNoteCount) - 1,
                totalNoteCount: Number(baselineModel.totalNoteCount) - 1
            }
            let expectedEditedUi = {
                totalNoteCount: Number(baselineUi.totalNoteCount) - 1
            }
            let expectedEditedModelState = JSON.stringify(expectedEditedModel)
            let expectedEditedUiState = JSON.stringify(expectedEditedUi)

            tryVerifyWithDiagnostics(() => {
                return snapshotModelState() === expectedEditedModelState
            }, 5000, "verify LiDAR note removed from model")
            tryVerifyWithDiagnostics(() => {
                return snapshotUiState() === expectedEditedUiState
            }, 5000, "verify LiDAR note removed from UI")
            tryVerifyWithDiagnostics(() => {
                return noteGallery().currentNoteLiDAR === null
                       || String(noteGallery().currentNoteLiDAR.filename) !== removedFileName
            }, 5000, "verify removed LiDAR note no longer selected")

            RootData.futureManagerModel.waitForFinished()
            wait(50)

            let saveResult = RootData.project.save()
            verify(saveResult)

            let commitAfterSave = TestHelper.projectHeadCommitOid(RootData.project)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")

            openTripPage(caveName, tripName)
            compare(snapshotModelState(), baselineModelState)
            compare(snapshotUiState(), baselineUiState)
            verifySelectedLiDARNote(removedFileName)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitC === commitB)

            openTripPage(caveName, tripName)
            compare(snapshotModelState(), expectedEditedModelState)
            compare(snapshotUiState(), expectedEditedUiState)
        }

        function test_tripRemoveImageNoteSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let currentTrip = function() {
                let currentPageItem = RootData.pageView.currentPageItem
                verify(currentPageItem !== null)
                verify(currentPageItem.currentTrip !== null)
                return currentPageItem.currentTrip
            }

            let notesModel = function() {
                let model = currentTrip().notes
                verify(model !== null)
                return model
            }

            let noteGalleryView = function() {
                let galleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView")
                verify(galleryView !== null)
                return galleryView
            }

            let snapshotModelState = function() {
                return JSON.stringify({
                    noteCount: notesModel().rowCount()
                })
            }

            let snapshotUiState = function() {
                return JSON.stringify({
                    noteCount: noteGalleryView().count
                })
            }

            let ensureAtLeastOneImageNote = function() {
                if (notesModel().rowCount() > 0) {
                    return
                }

                let copiedImagePath = TestHelper.copyToTempDir("://datasets/test_cwTextureUploadTask/PhakeCave.PNG")
                verify(copiedImagePath !== "")
                let copiedImageUrl = Qt.url("file://" + copiedImagePath)

                let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery")
                verify(noteGallery !== null)
                noteGallery.imagesAdded([copiedImageUrl])

                tryVerifyWithDiagnostics(() => {
                    return notesModel().rowCount() > 0
                }, 5000, "seed at least one image note")
            }

            let caveName = String(currentTrip().parentCave.name)
            let tripName = String(currentTrip().name)
            verify(caveName.length > 0)
            verify(tripName.length > 0)

            ensureAtLeastOneImageNote()
            let baselineModelState = snapshotModelState()
            let baselineUiState = snapshotUiState()
            let baselineModel = JSON.parse(baselineModelState)
            let baselineUi = JSON.parse(baselineUiState)
            verify(Number(baselineModel.noteCount) > 0)

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            notesModel().removeNote(notesModel().rowCount() - 1)

            let expectedEditedModel = {
                noteCount: Number(baselineModel.noteCount) - 1
            }
            let expectedEditedUi = {
                noteCount: Number(baselineUi.noteCount) - 1
            }
            let expectedEditedModelState = JSON.stringify(expectedEditedModel)
            let expectedEditedUiState = JSON.stringify(expectedEditedUi)

            tryVerifyWithDiagnostics(() => {
                return snapshotModelState() === expectedEditedModelState
            }, 5000, "verify note removed from model")
            tryVerifyWithDiagnostics(() => {
                return snapshotUiState() === expectedEditedUiState
            }, 5000, "verify note removed from UI")

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")

            openTripPage(caveName, tripName)
            compare(snapshotModelState(), baselineModelState)
            compare(snapshotUiState(), baselineUiState)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitC === commitB)

            openTripPage(caveName, tripName)
            compare(snapshotModelState(), expectedEditedModelState)
            compare(snapshotUiState(), expectedEditedUiState)
        }
    }
}
