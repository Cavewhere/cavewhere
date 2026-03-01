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
        name: "ScrapSync"
        when: windowShown

        function tryVerifyWithDiagnostics(predicate, timeoutMs, label, onPending) {
            SyncTestHelper.tryVerifyWithDiagnostics(testCaseId, predicate, timeoutMs, label, onPending)
        }

        function loadFixtureAndOpenFirstTrip() {
            return SyncTestHelper.loadFixtureAndOpenFirstTrip(testCaseId, RootData, TestHelper)
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

        function noteGalleryView() {
            let galleryView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->galleryView")
            verify(galleryView !== null)
            return galleryView
        }

        function carpetButton() {
            let button = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->carpetButtonId")
            verify(button !== null)
            return button
        }

        function addScrapButton() {
            let button = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addScrapButton")
            verify(button !== null)
            return button
        }

        function addScrapStationButton() {
            let button = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->addScrapStation")
            verify(button !== null)
            return button
        }

        function noteArea() {
            let area = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea")
            verify(area !== null)
            return area
        }

        function imageItem() {
            let image = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->imageId")
            verify(image !== null)
            return image
        }

        function scrapView() {
            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->scrapViewId")
            verify(view !== null)
            return view
        }

        function autoCalculateScrapCheckBox() {
            let checkBox = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->noteTransformEditor->autoCalculate->checkBox")
            verify(checkBox !== null)
            return checkBox
        }

        function findFirstNoteIndexWithScraps() {
            let model = currentTrip().notes
            verify(model !== null)

            for (let row = 0; row < model.rowCount(); ++row) {
                selectNoteIndex(row, "select note during scrap scan", true)

                if (TestHelper.noteScrapCount(noteGallery().currentNote) > 0) {
                    return row
                }
            }

            return -1
        }

        function findFirstNoteIndexWithoutScraps() {
            let model = currentTrip().notes
            verify(model !== null)

            for (let row = 0; row < model.rowCount(); ++row) {
                selectNoteIndex(row, "select note during empty scrap scan", true)

                if (TestHelper.noteScrapCount(noteGallery().currentNote) === 0) {
                    return row
                }
            }

            return -1
        }

        function selectNoteIndex(noteIndex, label, forceRebind) {
            let gallery = noteGallery()
            let galleryView = noteGalleryView()

            tryVerifyWithDiagnostics(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
                       && galleryView.count > noteIndex
            }, 5000, label + " wait for gallery row")

            let shouldForceRebind = forceRebind === true

            if (shouldForceRebind && galleryView.currentIndex === noteIndex) {
                galleryView.currentIndex = -1
                wait(50)
            }
            if (galleryView.currentIndex !== noteIndex) {
                galleryView.currentIndex = noteIndex
            }

            tryVerifyWithDiagnostics(() => {
                if (galleryView.currentIndex !== noteIndex) {
                    galleryView.currentIndex = noteIndex
                }

                if (shouldForceRebind
                        && gallery.currentNote === null
                        && galleryView.currentIndex === noteIndex
                        && gallery.state === "NO_NOTES") {
                    galleryView.currentIndex = -1
                    wait(50)
                    galleryView.currentIndex = noteIndex
                }

                return gallery.state !== "NO_NOTES"
                       && gallery.currentNote !== null
                       && galleryView.currentIndex === noteIndex
            }, 5000, label)
        }

        function enterCarpetMode() {
            let gallery = noteGallery()
            if (gallery.mode !== "CARPET") {
                mouseClick(carpetButton())
            }

            tryVerifyWithDiagnostics(() => {
                return gallery.mode === "CARPET"
            }, 5000, "enter carpet mode")
        }

        function waitForNoteCanvasReady(label) {
            let gallery = noteGallery()
            let area = noteArea()
            let image = imageItem()
            let galleryView = noteGalleryView()

            tryVerifyWithDiagnostics(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
                       && gallery.state !== "NO_NOTES"
                       && gallery.currentNote !== null
                       && galleryView.currentIndex >= 0
                       && area.visible === true
                       && image.visible === true
                       && image.width > 0
                       && image.height > 0
            }, 5000, label)
        }

        function disableNoteLoadUi() {
            let toolbarLoadButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->toolbarLoadNotesButton")
            if (toolbarLoadButton !== null) {
                if (toolbarLoadButton.visible !== undefined) {
                    toolbarLoadButton.visible = false
                }
                if (toolbarLoadButton.enabled !== undefined) {
                    toolbarLoadButton.enabled = false
                }
            }

            let emptyStateLoadButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->emptyStateLoadNotesButton")
            if (emptyStateLoadButton !== null) {
                if (emptyStateLoadButton.visible !== undefined) {
                    emptyStateLoadButton.visible = false
                }
                if (emptyStateLoadButton.enabled !== undefined) {
                    emptyStateLoadButton.enabled = false
                }
            }
        }

        function prepareScrapOutlineUi(stage) {
            let scrapNoteIndex = findFirstNoteIndexWithScraps()
            verify(scrapNoteIndex >= 0)

            let gallery = noteGallery()
            let galleryView = noteGalleryView()
            galleryView.currentIndex = scrapNoteIndex

            tryVerifyWithDiagnostics(() => {
                return gallery.currentNote !== null
            }, 5000, "select note with scraps")

            let requiresScrapSelection = stage === "before-set"
                                         || stage === "after-set"
                                         || stage === "after-set-ui"

            if (!requiresScrapSelection) {
                return
            }

            if (gallery.mode !== "CARPET") {
                mouseClick(carpetButton())
            }

            tryVerifyWithDiagnostics(() => {
                return gallery.mode === "CARPET"
            }, 5000, "enter carpet mode")

            let currentScrapView = scrapView()
            tryVerifyWithDiagnostics(() => {
                return currentScrapView.note === gallery.currentNote
            }, 5000, "bind scrap view to selected note")
            tryVerifyWithDiagnostics(() => {
                return currentScrapView.count > 0
            }, 5000, "wait for scraps in note area")

            if (currentScrapView.selectScrapIndex !== 0) {
                currentScrapView.selectScrapIndex = 0
            }

            tryVerifyWithDiagnostics(() => {
                return currentScrapView.selectedScrapItem !== null
                       && currentScrapView.selectedScrapItem.scrap !== null
            }, 5000, "select first scrap")
        }

        function restoreTripPage(tripPageAddress) {
            RootData.pageSelectionModel.currentPageAddress = tripPageAddress
            tryVerifyWithDiagnostics(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
            }, 10000, "restore trip page")
        }

        function selectedScrapItem() {
            let view = scrapView()
            if (view.selectedScrapItem === null && view.count > 0 && view.selectScrapIndex !== 0) {
                view.selectScrapIndex = 0
            }
            let item = view.selectedScrapItem
            verify(item !== null)
            return item
        }

        function snapshotSelectedScrapOutlineState() {
            let gallery = noteGallery()
            verify(gallery.currentNote !== null)
            let state = TestHelper.scrapOutlineState(gallery.currentNote, 0)
            verify(state !== null && state !== undefined)
            return state
        }

        function mapOutlineStateToRenderedState(state) {
            let note = noteGallery().currentNote
            verify(note !== null)

            let renderSize = note.renderSize()
            let renderedState = {
                pointCount: state.pointCount,
                points: []
            }

            for (let i = 0; i < state.points.length; ++i) {
                let point = state.points[i]
                renderedState.points.push({
                    index: point.index,
                    x: point.x * renderSize.width,
                    y: (1.0 - point.y) * renderSize.height
                })
            }

            return renderedState
        }

        function snapshotSelectedScrapStationState() {
            let scrap = selectedScrapItem().scrap
            verify(scrap !== null)

            let state = {
                stationCount: scrap.numberOfStations(),
                stations: []
            }

            for (let i = 0; i < scrap.numberOfStations(); ++i) {
                let position = scrap.stationData(Scrap.StationPosition, i)
                state.stations.push({
                    index: i,
                    name: String(scrap.stationData(Scrap.StationName, i)),
                    x: Number(position.x.toFixed(4)),
                    y: Number(position.y.toFixed(4))
                })
            }

            return state
        }

        function snapshotSelectedScrapRenderedOutlineState() {
            let state = selectedScrapItem().renderedOutlineState()
            verify(state !== null && state !== undefined)
            return state
        }

        function cloneOutlineState(state) {
            let clonedState = {
                pointCount: state.pointCount,
                points: []
            }

            for (let i = 0; i < state.points.length; ++i) {
                let point = state.points[i]
                clonedState.points.push({
                    index: point.index,
                    x: point.x,
                    y: point.y
                })
            }

            return clonedState
        }

        function isClosedOutlineState(state) {
            if (state.points.length < 2) {
                return false
            }

            let firstPoint = state.points[0]
            let lastPoint = state.points[state.points.length - 1]
            return firstPoint.x === lastPoint.x && firstPoint.y === lastPoint.y
        }

        function applySelectedScrapOutlineState(state) {
            let scrapItem = selectedScrapItem()
            let scrap = scrapItem.scrap
            verify(scrap !== null)

            while (scrap.numberOfPoints() < state.points.length) {
                let insertIndex = isClosedOutlineState(state)
                                ? scrap.numberOfPoints() - 1
                                : scrap.numberOfPoints()
                let insertedPoint = state.points[insertIndex]
                scrap.insertPoint(insertIndex, Qt.point(insertedPoint.x, insertedPoint.y))
            }

            while (scrap.numberOfPoints() > state.points.length) {
                let removeIndex = scrap.isClosed()
                                ? scrap.numberOfPoints() - 2
                                : scrap.numberOfPoints() - 1
                scrap.removePoint(removeIndex)
            }

            compare(scrap.numberOfPoints(), state.points.length)

            for (let i = 0; i < state.points.length; ++i) {
                let point = state.points[i]
                scrap.setPoint(i, Qt.point(point.x, point.y))
            }
        }

        function nextOutlineState(state) {
            verify(state.points.length > 1)

            let nextState = cloneOutlineState(state)

            nextState.points[1].x += 0.01
            nextState.points[1].y += 0.01
            return nextState
        }

        function addThreeOutlinePointsState(state) {
            verify(state.points.length > 2)

            let nextState = cloneOutlineState(state)
            let insertionBaseIndex = isClosedOutlineState(nextState)
                                   ? nextState.points.length - 1
                                   : nextState.points.length

            let anchorA = nextState.points[0]
            let anchorB = nextState.points[1]
            let deltaX = anchorB.x - anchorA.x
            let deltaY = anchorB.y - anchorA.y
            let offsetScale = 0.005

            let insertedPoints = [
                { x: anchorA.x + deltaX * 0.25 - deltaY * offsetScale, y: anchorA.y + deltaY * 0.25 + deltaX * offsetScale },
                { x: anchorA.x + deltaX * 0.50 - deltaY * offsetScale, y: anchorA.y + deltaY * 0.50 + deltaX * offsetScale },
                { x: anchorA.x + deltaX * 0.75 - deltaY * offsetScale, y: anchorA.y + deltaY * 0.75 + deltaX * offsetScale }
            ]

            for (let i = 0; i < insertedPoints.length; ++i) {
                nextState.points.splice(insertionBaseIndex + i, 0, {
                    index: insertionBaseIndex + i,
                    x: insertedPoints[i].x,
                    y: insertedPoints[i].y
                })
            }

            for (let pointIndex = 0; pointIndex < nextState.points.length; ++pointIndex) {
                nextState.points[pointIndex].index = pointIndex
            }
            nextState.pointCount = nextState.points.length
            return nextState
        }

        function removeLastThreeOutlinePointsState(state) {
            verify(state.points.length >= 6)

            let nextState = cloneOutlineState(state)
            let removeStartIndex = isClosedOutlineState(nextState)
                                 ? nextState.points.length - 4
                                 : nextState.points.length - 3
            nextState.points.splice(removeStartIndex, 3)

            for (let pointIndex = 0; pointIndex < nextState.points.length; ++pointIndex) {
                nextState.points[pointIndex].index = pointIndex
            }
            nextState.pointCount = nextState.points.length
            return nextState
        }

        function nextStationStateWithAddedStation(state) {
            let nextState = {
                stationCount: state.stationCount + 1,
                stations: []
            }

            for (let i = 0; i < state.stations.length; ++i) {
                let station = state.stations[i]
                nextState.stations.push({
                    index: station.index,
                    name: station.name,
                    x: station.x,
                    y: station.y
                })
            }

            nextState.stations.push({
                index: nextState.stations.length,
                name: "sync-added-station",
                x: 0.52,
                y: 0.58
            })

            return nextState
        }

        function nextStationStateWithRemovedStation(state) {
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
                    x: station.x,
                    y: station.y
                })
            }

            return nextState
        }

        function nextStationStateWithRenamedStation(state) {
            verify(state.stationCount > 0)

            let nextState = {
                stationCount: state.stationCount,
                stations: []
            }

            for (let i = 0; i < state.stations.length; ++i) {
                let station = state.stations[i]
                nextState.stations.push({
                    index: i,
                    name: station.name,
                    x: station.x,
                    y: station.y
                })
            }

            let targetIndex = nextState.stations.length - 1
            let baselineName = nextState.stations[targetIndex].name
            nextState.stations[targetIndex].name =
                    baselineName === "syncrenamedstation"
                    ? "syncrenamedstation2"
                    : "syncrenamedstation"

            return nextState
        }

        function renderedSelectedScrapStationNames() {
            let stationView = selectedScrapItem().stationView
            let names = []
            let objectNames = stationView.itemObjectNames()
            for (let i = 0; i < objectNames.length; ++i) {
                let objectName = String(objectNames[i])
                if (objectName.startsWith("station")) {
                    names.push(objectName.slice("station".length))
                }
            }
            names.sort()
            return names
        }

        function applySelectedScrapStationState(state) {
            let scrap = selectedScrapItem().scrap
            verify(scrap !== null)
            let currentCount = scrap.numberOfStations()

            if (state.stations.length === currentCount + 1) {
                let note = noteGallery().currentNote
                verify(note !== null)
                let addedStation = state.stations[state.stations.length - 1]

                verify(TestHelper.addScrapStation(note, 0, addedStation.name, Qt.point(addedStation.x, addedStation.y)))

                tryVerifyWithDiagnostics(() => {
                    return scrap.numberOfStations() === state.stations.length
                }, 5000, "wait for added scrap station")
            } else if (state.stations.length === currentCount - 1) {
                scrap.removeStation(currentCount - 1)

                tryVerifyWithDiagnostics(() => {
                    return scrap.numberOfStations() === state.stations.length
                }, 5000, "wait for removed scrap station")
            } else {
                compare(state.stations.length, currentCount)
            }

            compare(scrap.numberOfStations(), state.stations.length)

            for (let i = 0; i < state.stations.length; ++i) {
                let expectedStation = state.stations[i]
                let currentName = String(scrap.stationData(Scrap.StationName, i))
                let currentPosition = scrap.stationData(Scrap.StationPosition, i)
                if (currentName !== expectedStation.name) {
                    scrap.setStationData(Scrap.StationName, i, expectedStation.name)
                }
                if (Number(currentPosition.x.toFixed(4)) !== expectedStation.x
                        || Number(currentPosition.y.toFixed(4)) !== expectedStation.y) {
                    scrap.setStationData(Scrap.StationPosition, i, Qt.point(expectedStation.x, expectedStation.y))
                }
            }

            tryVerifyWithDiagnostics(() => {
                let currentState = snapshotSelectedScrapStationState()
                return SyncTestHelper.deepEqual(currentState, state)
            }, 5000, "wait for applied scrap station state")
        }

        function createNewOpenScrapWithThreePoints(existingScrapCount) {
            disableNoteLoadUi()
            waitForNoteCanvasReady("wait for note canvas before creating scrap")

            let currentScrapView = scrapView()
            compare(currentScrapView.count, existingScrapCount)
            if (currentScrapView.selectScrapIndex !== -1) {
                currentScrapView.selectScrapIndex = -1
            }

            mouseClick(addScrapButton())
            noteGallery().state = "ADD-SCRAP"
            wait(100)

            let image = imageItem()
            let points = [
                { x: image.width * 0.30, y: image.height * 0.35 },
                { x: image.width * 0.55, y: image.height * 0.36 },
                { x: image.width * 0.48, y: image.height * 0.62 }
            ]

            mouseMove(image, points[0].x, points[0].y)
            mouseClick(image, points[0].x, points[0].y)
            wait(50)
            tryVerifyWithDiagnostics(() => {
                return currentScrapView.count === existingScrapCount + 1
            }, 5000, "wait for new scrap row")
            mouseClick(autoCalculateScrapCheckBox())
            wait(50)

            for (let i = 1; i < points.length; ++i) {
                mouseMove(image, points[i].x, points[i].y)
                mouseClick(image, points[i].x, points[i].y)
                wait(50)
            }

            tryVerifyWithDiagnostics(() => {
                return currentScrapView.selectedScrapItem !== null
                       && currentScrapView.selectedScrapItem.scrap !== null
                       && currentScrapView.selectedScrapItem.scrap.numberOfPoints() === 3
            }, 5000, "wait for three-point scrap")
        }

        function test_existingScrapOutlineSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapOutlineUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapOutlineState,
                uiExpectedFromValue: mapOutlineStateToRenderedState,
                uiGetter: () => {
                    return snapshotSelectedScrapRenderedOutlineState()
                },
                verifyBaselineUi: false,
                verifyEditedUi: false,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapOutlineState,
                nextValue: nextOutlineState
            })
        }

        function test_scrapOutlineAddThenRemovePointsSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapOutlineUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapOutlineState,
                uiExpectedFromValue: mapOutlineStateToRenderedState,
                uiGetter: () => {
                    return snapshotSelectedScrapRenderedOutlineState()
                },
                verifyBaselineUi: false,
                verifyEditedUi: false,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapOutlineState,
                nextValue: addThreeOutlinePointsState
            })

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareScrapOutlineUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapOutlineState,
                uiExpectedFromValue: mapOutlineStateToRenderedState,
                uiGetter: () => {
                    return snapshotSelectedScrapRenderedOutlineState()
                },
                verifyBaselineUi: false,
                verifyEditedUi: false,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapOutlineState,
                nextValue: removeLastThreeOutlinePointsState
            })
        }

        function test_newScrapCreateThenRemoveSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let newScrapNoteIndex = findFirstNoteIndexWithScraps()
            verify(newScrapNoteIndex >= 0)
            selectNoteIndex(newScrapNoteIndex, "select note for new scrap baseline")
            let baselineExistingScrapCount = TestHelper.noteScrapCount(noteGallery().currentNote)
            verify(baselineExistingScrapCount > 0)

            let prepareNewScrapUi = function(stage) {
                disableNoteLoadUi()
                selectNoteIndex(newScrapNoteIndex, "select note for new scrap test")
                waitForNoteCanvasReady("wait for note canvas in new scrap test")

                let requiresCarpetMode = stage === "before-set"
                                       || stage === "after-set"
                                       || stage === "after-set-ui"
                if (!requiresCarpetMode) {
                    return
                }

                enterCarpetMode()

                let currentScrapView = scrapView()
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.note === noteGallery().currentNote
                }, 5000, "bind scrap view to empty note")

                if (currentScrapView.count > baselineExistingScrapCount) {
                    let createdScrapIndex = currentScrapView.count - 1
                    if (currentScrapView.selectScrapIndex !== createdScrapIndex) {
                        currentScrapView.selectScrapIndex = createdScrapIndex
                    }
                } else if (currentScrapView.count > 0 && currentScrapView.selectScrapIndex !== 0) {
                    currentScrapView.selectScrapIndex = 0
                }

                if (currentScrapView.count > baselineExistingScrapCount) {
                    tryVerifyWithDiagnostics(() => {
                        return currentScrapView.selectedScrapItem !== null
                               && currentScrapView.selectedScrapItem.scrap !== null
                    }, 5000, "select new scrap item")
                }
            }

            let snapshotNewScrapState = function() {
                disableNoteLoadUi()
                selectNoteIndex(newScrapNoteIndex, "select note for new scrap snapshot")
                waitForNoteCanvasReady("wait for note canvas before new scrap snapshot")
                let note = noteGallery().currentNote
                verify(note !== null)

                let scrapCount = TestHelper.noteScrapCount(note)
                let state = {
                    scrapCount: scrapCount,
                    outlineState: null
                }

                if (scrapCount > baselineExistingScrapCount) {
                    state.outlineState = TestHelper.scrapOutlineState(note, scrapCount - 1)
                }

                return state
            }

            let applyNewScrapState = function(state) {
                disableNoteLoadUi()
                waitForNoteCanvasReady("wait for note canvas before applying new scrap state")
                enterCarpetMode()

                let currentScrapView = scrapView()
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.note === noteGallery().currentNote
                }, 5000, "bind scrap view while applying new scrap state")

                if (state.scrapCount === baselineExistingScrapCount) {
                    if (currentScrapView.count === baselineExistingScrapCount) {
                        return
                    }

                    let createdScrapIndex = currentScrapView.count - 1
                    if (currentScrapView.selectScrapIndex !== createdScrapIndex) {
                        currentScrapView.selectScrapIndex = createdScrapIndex
                    }

                    tryVerifyWithDiagnostics(() => {
                        return currentScrapView.selectedScrapItem !== null
                               && currentScrapView.selectedScrapItem.scrap !== null
                    }, 5000, "select scrap before deleting")

                    let scrap = currentScrapView.selectedScrapItem.scrap
                    while (scrap !== null && currentScrapView.count > 0 && scrap.numberOfPoints() > 0) {
                        scrap.removePoint(scrap.numberOfPoints() - 1)
                    }

                    tryVerifyWithDiagnostics(() => {
                        return currentScrapView.count === baselineExistingScrapCount
                    }, 5000, "wait for deleted scrap")
                    return
                }

                if (currentScrapView.count === baselineExistingScrapCount) {
                    createNewOpenScrapWithThreePoints(baselineExistingScrapCount)
                }

                let createdScrapIndex = currentScrapView.count - 1
                if (currentScrapView.selectScrapIndex !== createdScrapIndex) {
                    currentScrapView.selectScrapIndex = createdScrapIndex
                }

                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.selectedScrapItem !== null
                           && currentScrapView.selectedScrapItem.scrap !== null
                }, 5000, "select created scrap")

                applySelectedScrapOutlineState(state.outlineState)
            }

            let createThreePointScrapState = function() {
                return {
                    scrapCount: baselineExistingScrapCount + 1,
                    outlineState: {
                        pointCount: 3,
                        points: [
                            { index: 0, x: 0.30, y: 0.35 },
                            { index: 1, x: 0.55, y: 0.36 },
                            { index: 2, x: 0.48, y: 0.62 }
                        ]
                    }
                }
            }

            let removeCreatedScrapState = function() {
                return {
                    scrapCount: baselineExistingScrapCount,
                    outlineState: null
                }
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareNewScrapUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotNewScrapState,
                verifyBaselineUi: false,
                verifyEditedUi: false,
                verifyCheckoutUi: false,
                verifyResyncUi: false,
                setter: applyNewScrapState,
                nextValue: createThreePointScrapState
            })

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareNewScrapUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotNewScrapState,
                verifyBaselineUi: false,
                verifyEditedUi: false,
                verifyCheckoutUi: false,
                verifyResyncUi: false,
                setter: applyNewScrapState,
                nextValue: removeCreatedScrapState
            })
        }

        function test_existingScrapAddStationSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let addedStationName = "syncaddedstation"
            let prepareStationUi = function(stage) {
                prepareScrapOutlineUi(stage)

                let needsSelectionForUi = stage === "after-set-ui"
                                       || stage === "after-checkout-ui"
                                       || stage === "after-resync-ui"
                if (!needsSelectionForUi) {
                    return
                }

                let currentScrapView = scrapView()
                if (currentScrapView.selectScrapIndex !== 0) {
                    currentScrapView.selectScrapIndex = 0
                }
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.selectedScrapItem !== null
                           && currentScrapView.selectedScrapItem.scrap !== null
                }, 5000, "select scrap for station UI assertions")
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareStationUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapStationState,
                uiExpectedFromValue: (state) => {
                    return state.stationCount
                },
                uiGetter: () => {
                    return selectedScrapItem().stationView.count
                },
                verifyBaselineUi: true,
                verifyEditedUi: true,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapStationState,
                nextValue: (state) => {
                    let nextState = nextStationStateWithAddedStation(state)
                    nextState.stations[nextState.stations.length - 1].name = addedStationName
                    return nextState
                }
            })
        }

        function test_existingScrapRemoveStationSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let prepareStationUi = function(stage) {
                prepareScrapOutlineUi(stage)

                let needsSelectionForUi = stage === "after-set-ui"
                                       || stage === "after-checkout-ui"
                                       || stage === "after-resync-ui"
                if (!needsSelectionForUi) {
                    return
                }

                let currentScrapView = scrapView()
                if (currentScrapView.selectScrapIndex !== 0) {
                    currentScrapView.selectScrapIndex = 0
                }
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.selectedScrapItem !== null
                           && currentScrapView.selectedScrapItem.scrap !== null
                }, 5000, "select scrap for station UI assertions")
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareStationUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapStationState,
                uiExpectedFromValue: (state) => {
                    return state.stationCount
                },
                uiGetter: () => {
                    return selectedScrapItem().stationView.count
                },
                verifyBaselineUi: true,
                verifyEditedUi: true,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapStationState,
                nextValue: nextStationStateWithRemovedStation
            })
        }

        function test_existingScrapRenameStationSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()
            let prepareStationUi = function(stage) {
                prepareScrapOutlineUi(stage)

                let needsSelectionForUi = stage === "after-set-ui"
                                       || stage === "after-checkout-ui"
                                       || stage === "after-resync-ui"
                if (!needsSelectionForUi) {
                    return
                }

                let currentScrapView = scrapView()
                if (currentScrapView.selectScrapIndex !== 0) {
                    currentScrapView.selectScrapIndex = 0
                }
                tryVerifyWithDiagnostics(() => {
                    return currentScrapView.selectedScrapItem !== null
                           && currentScrapView.selectedScrapItem.scrap !== null
                }, 5000, "select scrap for station rename UI assertions")
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                prepare: prepareStationUi,
                restorePage: () => restoreTripPage(context.tripPageAddress),
                getter: snapshotSelectedScrapStationState,
                uiExpectedFromValue: (state) => {
                    let names = []
                    for (let i = 0; i < state.stations.length; ++i) {
                        names.push(state.stations[i].name)
                    }
                    names.sort()
                    return names
                },
                uiGetter: () => {
                    return renderedSelectedScrapStationNames()
                },
                verifyBaselineUi: true,
                verifyEditedUi: true,
                verifyCheckoutUi: true,
                verifyResyncUi: true,
                setter: applySelectedScrapStationState,
                nextValue: nextStationStateWithRenamedStation
            })
        }
    }
}
