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
        name: "LiDARSync"
        when: windowShown

        function cleanup() {
            RootData.pageSelectionModel.gotoPageByName(null, "View")
            tryVerifyWithDiagnostics(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "viewPage"
            }, 5000, "return to view page during cleanup")
            RootData.newProject()
        }

        function tryVerifyWithDiagnostics(predicate, timeoutMs, label, onPending) {
            SyncTestHelper.tryVerifyWithDiagnostics(testCaseId, predicate, timeoutMs, label, onPending)
        }

        function loadFixtureAndOpenFirstTrip() {
            let fixture = TestHelper.createLocalLiDARSyncFixtureWithLfsServer()
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
                if (candidateCave === null) {
                    continue
                }
                for (let j = 0; j < candidateCave.rowCount(); ++j) {
                    let candidateTrip = candidateCave.trip(j)
                    if (candidateTrip !== null
                            && candidateTrip.notesLiDAR !== null
                            && candidateTrip.notesLiDAR.rowCount() > 0) {
                        cave = candidateCave
                        trip = candidateTrip
                        break
                    }
                }
                if (trip !== null) {
                    break
                }
            }

            verify(cave !== null)
            verify(trip !== null)

            let caveName = String(cave.name)
            let tripName = String(trip.name)
            verify(caveName.length > 0)
            verify(tripName.length > 0)

            SyncTestHelper.openTripPage(testCaseId, RootData, caveName, tripName)

            return {
                tripPageAddress: "Source/Data/Cave=" + caveName + "/Trip=" + tripName
            }
        }

        function currentTrip() {
            let currentPageItem = RootData.pageView.currentPageItem
            verify(currentPageItem !== null)
            verify(currentPageItem.currentTrip !== null)
            return currentPageItem.currentTrip
        }

        function noteGallery() {
            let gallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery")
            verify(gallery !== null)
            return gallery
        }

        function noteGalleryView() {
            let galleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView")
            verify(galleryView !== null)
            return galleryView
        }

        function noteLiDARViewer() {
            let viewer = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->rhiViewerId")
            verify(viewer !== null)
            return viewer
        }

        function currentLiDARNote() {
            let note = noteGallery().currentNoteLiDAR
            verify(note !== null)
            return note
        }

        function roundNumber(value) {
            return Number(Number(value).toFixed(4))
        }

        function findDescendantsWhere(rootObject, predicate, matches) {
            if (matches === undefined || matches === null) {
                matches = []
            }
            if (rootObject === null || rootObject === undefined) {
                return matches
            }
            if (predicate(rootObject)) {
                matches.push(rootObject)
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
                    findDescendantsWhere(children[i], predicate, matches)
                }
            }

            return matches
        }

        function selectLiDARNoteByFilename(fileName) {
            let gallery = noteGallery()
            let galleryView = noteGalleryView()
            tryVerifyWithDiagnostics(() => {
                return galleryView.count > 0
            }, 10000, "wait for LiDAR note gallery items")

            galleryView.currentIndex = -1
            wait(50)

            for (let i = 0; i < galleryView.count; ++i) {
                let galleryItem = ObjectFinder.findObjectByChain(
                    mainWindow,
                    "rootId->tripPage->noteGallery->galleryView->noteImage" + i)
                if (galleryItem === null) {
                    continue
                }
                mouseClick(galleryItem)
                wait(50)
                if (gallery.currentNoteLiDAR !== null
                        && String(gallery.currentNoteLiDAR.filename) === fileName) {
                    break
                }
            }

            tryVerifyWithDiagnostics(() => {
                return gallery.currentNoteLiDAR !== null
                       && String(gallery.currentNoteLiDAR.filename) === fileName
            }, 10000, "select LiDAR note by filename")

            tryVerifyWithDiagnostics(() => {
                return noteLiDARViewer().scene.gltf.status === RenderGLTF.Ready
            }, 10000, "wait for LiDAR viewer ready")
        }

        function prepareLiDARNoteUi(fileName) {
            SyncTestHelper.waitForFutureManagerToFinish(testCaseId, RootData)
            selectLiDARNoteByFilename(fileName)
            SyncTestHelper.waitForFutureManagerToFinish(testCaseId, RootData)
            tryVerifyWithDiagnostics(() => {
                let note = noteGallery().currentNoteLiDAR
                if (note === null || String(note.filename) !== fileName) {
                    return false
                }

                return noteLiDARViewer().scene.gltf.status === RenderGLTF.Ready
                       && TestHelper.liDARStationLookupSize(note) > 0
                       && TestHelper.liDARSurveyNetworkIsEmpty(note) === false
                       && selectedLiDARStationUiState().stationCount === note.rowCount()
            }, 10000, "wait for LiDAR note ready", () => {
                SyncTestHelper.waitForFutureManagerToFinish(testCaseId, RootData)
            })
        }

        function selectAnyLiDARNote() {
            let gallery = noteGallery()
            let galleryView = noteGalleryView()
            tryVerifyWithDiagnostics(() => {
                return galleryView.count > 0
            }, 10000, "wait for any LiDAR note gallery items")

            galleryView.currentIndex = -1
            wait(50)

            for (let i = 0; i < galleryView.count; ++i) {
                let galleryItem = ObjectFinder.findObjectByChain(
                    mainWindow,
                    "rootId->tripPage->noteGallery->galleryView->noteImage" + i)
                if (galleryItem === null) {
                    continue
                }
                mouseClick(galleryItem)
                wait(50)
                if (gallery.currentNoteLiDAR !== null) {
                    break
                }
            }

            tryVerifyWithDiagnostics(() => {
                return gallery.currentNoteLiDAR !== null
            }, 10000, "select any LiDAR note")

            tryVerifyWithDiagnostics(() => {
                return noteLiDARViewer().scene.gltf.status === RenderGLTF.Ready
            }, 10000, "wait for any LiDAR viewer ready")

            return String(gallery.currentNoteLiDAR.filename)
        }

        function snapshotSelectedLiDARStationState() {
            let note = currentLiDARNote()
            let stations = []
            for (let i = 0; i < note.rowCount(); ++i) {
                let modelIndex = note.index(i, 0)
                let positionOnNote = note.data(modelIndex, NoteLiDAR.PositionOnNoteRole)
                stations.push({
                    index: i,
                    name: String(note.data(modelIndex, NoteLiDAR.NameRole)),
                    positionOnNote: {
                        x: roundNumber(positionOnNote.x),
                        y: roundNumber(positionOnNote.y),
                        z: roundNumber(positionOnNote.z)
                    }
                })
            }

            return {
                stationCount: note.rowCount(),
                stations: stations
            }
        }

        function selectedLiDARStationUiState() {
            let stationItems = findDescendantsWhere(noteLiDARViewer(), (child) => {
                return child !== null
                       && child.objectName !== undefined
                       && String(child.objectName).startsWith("noteLiDARStation_")
                       && child.visible === true
            })

            stationItems.sort((lhs, rhs) => {
                return String(lhs.objectName).localeCompare(String(rhs.objectName))
            })

            let stationNames = []
            let stationPositions = []
            for (let i = 0; i < stationItems.length; ++i) {
                stationNames.push(String(stationItems[i].name))
                stationPositions.push({
                    x: roundNumber(stationItems[i].position3D.x),
                    y: roundNumber(stationItems[i].position3D.y),
                    z: roundNumber(stationItems[i].position3D.z)
                })
            }

            return {
                stationCount: stationItems.length,
                stationNames: stationNames,
                stationPositions: stationPositions
            }
        }

        function expectedLiDARStationUiState(state) {
            let note = currentLiDARNote()
            let stationNames = []
            let stationPositions = []
            for (let i = 0; i < state.stations.length; ++i) {
                let station = state.stations[i]
                stationNames.push(station.name)

                let modelIndex = note.index(i, 0)
                let upPosition = note.data(modelIndex, NoteLiDAR.UpPositionRole)
                stationPositions.push({
                    x: roundNumber(upPosition.x),
                    y: roundNumber(upPosition.y),
                    z: roundNumber(upPosition.z)
                })
            }

            return {
                stationCount: state.stationCount,
                stationNames: stationNames,
                stationPositions: stationPositions
            }
        }

        function applySelectedLiDARStationState(state) {
            let note = currentLiDARNote()
            let currentCount = note.rowCount()

            if (state.stationCount === currentCount + 1) {
                let addedStation = state.stations[state.stations.length - 1]
                verify(TestHelper.addLiDARStation(
                           note,
                           addedStation.name,
                           Qt.vector3d(0.1, 0.2, 0.3)))
            } else if (state.stationCount === currentCount - 1) {
                note.removeStation(currentCount - 1)
            } else {
                compare(state.stationCount, currentCount)
                for (let i = 0; i < currentCount; ++i) {
                    let expectedStation = state.stations[i]
                    let modelIndex = note.index(i, 0)
                    let currentName = String(note.data(modelIndex, NoteLiDAR.NameRole))
                    let currentPosition = note.data(modelIndex, NoteLiDAR.PositionOnNoteRole)

                    if (currentName !== expectedStation.name) {
                        verify(note.setData(modelIndex, expectedStation.name, NoteLiDAR.NameRole))
                    }

                    if (roundNumber(currentPosition.x) !== expectedStation.positionOnNote.x
                            || roundNumber(currentPosition.y) !== expectedStation.positionOnNote.y
                            || roundNumber(currentPosition.z) !== expectedStation.positionOnNote.z) {
                        verify(note.setData(
                                   modelIndex,
                                   Qt.vector3d(expectedStation.positionOnNote.x,
                                               expectedStation.positionOnNote.y,
                                               expectedStation.positionOnNote.z),
                                   NoteLiDAR.PositionOnNoteRole))
                    }
                }
            }

            tryVerifyWithDiagnostics(() => {
                return SyncTestHelper.deepEqual(snapshotSelectedLiDARStationState(), state)
            }, 5000, "wait for applied LiDAR station state")
            TestHelper.waitForProjectSaveToFinish(RootData.project)
        }

        function nextLiDARStationStateWithAddedStation(state) {
            let excludedNames = []
            for (let i = 0; i < state.stations.length; ++i) {
                excludedNames.push(state.stations[i].name)
            }
            let addedStationName = TestHelper.firstUnusedTripStationName(currentTrip(), excludedNames)
            verify(addedStationName.length > 0)

            let nextState = {
                stationCount: state.stationCount + 1,
                stations: []
            }

            for (let i = 0; i < state.stations.length; ++i) {
                let station = state.stations[i]
                nextState.stations.push({
                    index: station.index,
                    name: station.name,
                    positionOnNote: station.positionOnNote
                })
            }

            nextState.stations.push({
                index: nextState.stations.length,
                name: addedStationName,
                positionOnNote: {
                    x: 0.1,
                    y: 0.2,
                    z: 0.3
                }
            })

            return nextState
        }

        function nextLiDARStationStateWithRemovedStation(state) {
            verify(state.stationCount > 0)

            let nextState = {
                stationCount: state.stationCount - 1,
                stations: []
            }

            for (let i = 0; i < state.stations.length - 1; ++i) {
                let station = state.stations[i]
                nextState.stations.push({
                    index: i,
                    name: station.name,
                    positionOnNote: station.positionOnNote
                })
            }

            return nextState
        }

        function nextLiDARStationStateWithRenamedStation(state) {
            verify(state.stationCount > 0)

            let excludedNames = []
            for (let i = 0; i < state.stations.length; ++i) {
                excludedNames.push(state.stations[i].name)
            }
            let renamedStationName = TestHelper.firstUnusedTripStationName(currentTrip(), excludedNames)
            verify(renamedStationName.length > 0)

            let nextState = {
                stationCount: state.stationCount,
                stations: []
            }

            for (let i = 0; i < state.stations.length; ++i) {
                let station = state.stations[i]
                nextState.stations.push({
                    index: station.index,
                    name: i === 0 ? renamedStationName : station.name,
                    positionOnNote: station.positionOnNote
                })
            }

            return nextState
        }

        function test_existingLiDARAddStationSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let targetFileName = selectAnyLiDARNote()
            verify(targetFileName.length > 0)

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                getter: snapshotSelectedLiDARStationState,
                uiGetter: selectedLiDARStationUiState,
                uiExpectedFromValue: expectedLiDARStationUiState,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                nextValue: nextLiDARStationStateWithAddedStation,
                setter: applySelectedLiDARStationState,
                prepare: function() {
                    prepareLiDARNoteUi(targetFileName)
                }
            })
        }

        function test_existingLiDARRemoveStationSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let targetFileName = selectAnyLiDARNote()
            verify(targetFileName.length > 0)

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                getter: snapshotSelectedLiDARStationState,
                uiGetter: selectedLiDARStationUiState,
                uiExpectedFromValue: expectedLiDARStationUiState,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                nextValue: nextLiDARStationStateWithRemovedStation,
                setter: applySelectedLiDARStationState,
                prepare: function() {
                    prepareLiDARNoteUi(targetFileName)
                }
            })
        }

        function test_existingLiDARRenameStationSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let targetFileName = selectAnyLiDARNote()
            verify(targetFileName.length > 0)

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                getter: snapshotSelectedLiDARStationState,
                uiGetter: selectedLiDARStationUiState,
                uiExpectedFromValue: expectedLiDARStationUiState,
                verifyBaselineAfterCheckoutTimeoutMs: 10000,
                verifyResyncedValueTimeoutMs: 10000,
                nextValue: nextLiDARStationStateWithRenamedStation,
                setter: applySelectedLiDARStationState,
                prepare: function() {
                    prepareLiDARNoteUi(targetFileName)
                }
            })
        }

    }
}
