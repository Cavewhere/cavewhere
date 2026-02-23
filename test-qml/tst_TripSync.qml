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

        function runTripSyncRoundTrip(tripPageAddress, getter, setter, nextValue, uiGetter) {
            const verifyEditedValueTimeoutMs = 3000
            const verifyBaselineAfterCheckoutTimeoutMs = 3000
            const verifyResyncedValueTimeoutMs = 3000

            let baselineValue = String(getter())
            verify(baselineValue.length > 0)
            if (uiGetter !== undefined && uiGetter !== null) {
                tryVerifyWithDiagnostics(() => {
                    return String(uiGetter()) === baselineValue
                }, verifyEditedValueTimeoutMs, "verify baseline UI value")
            }

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            let syncedValue = String(nextValue(baselineValue))
            setter(syncedValue)
            tryVerifyWithDiagnostics(() => {
                return String(getter()) === syncedValue
            }, verifyEditedValueTimeoutMs, "verify value after local setter")
            if (uiGetter !== undefined && uiGetter !== null) {
                tryVerifyWithDiagnostics(() => {
                    return String(uiGetter()) === syncedValue
                }, verifyEditedValueTimeoutMs, "verify edited UI value")
            }

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
            if (uiGetter !== undefined && uiGetter !== null) {
                tryVerifyWithDiagnostics(() => {
                    return String(uiGetter()) === baselineValue
                }, verifyBaselineAfterCheckoutTimeoutMs, "verify baseline UI after checkout")
            }

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitB === commitC)

            verifyStillOnTripPage(tripPageAddress)
            tryVerifyWithDiagnostics(() => {
                return String(getter()) === syncedValue
            }, verifyResyncedValueTimeoutMs, "verify synced value after second sync")
            if (uiGetter !== undefined && uiGetter !== null) {
                tryVerifyWithDiagnostics(() => {
                    return String(uiGetter()) === syncedValue
                }, verifyResyncedValueTimeoutMs, "verify synced UI value after second sync")
            }
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
    }
}
