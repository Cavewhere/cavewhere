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

        function noteArea() {
            let area = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea")
            verify(area !== null)
            return area
        }

        function scrapView() {
            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->noteArea->scrapViewId")
            verify(view !== null)
            return view
        }

        function findFirstNoteIndexWithScraps() {
            let model = currentTrip().notes
            verify(model !== null)
            let gallery = noteGallery()
            let galleryView = noteGalleryView()

            for (let row = 0; row < model.rowCount(); ++row) {
                galleryView.currentIndex = row
                tryVerifyWithDiagnostics(() => {
                    return gallery.currentNote !== null && galleryView.currentIndex === row
                }, 5000, "select note during scrap scan")

                if (TestHelper.noteScrapCount(gallery.currentNote) > 0) {
                    return row
                }
            }

            return -1
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
            let item = scrapView().selectedScrapItem
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

        function snapshotSelectedScrapRenderedOutlineState() {
            let state = selectedScrapItem().renderedOutlineState()
            verify(state !== null && state !== undefined)
            return state
        }

        function applySelectedScrapOutlineState(state) {
            verify(state.points.length > 1)

            let movedPoint = state.points[1]
            let scrapItem = selectedScrapItem()
            scrapItem.scrap.setPoint(movedPoint.index, Qt.point(movedPoint.x, movedPoint.y))
        }

        function nextOutlineState(state) {
            verify(state.points.length > 1)

            let nextState = {
                pointCount: state.pointCount,
                points: []
            }

            for (let i = 0; i < state.points.length; ++i) {
                let point = state.points[i]
                nextState.points.push({
                    index: point.index,
                    x: point.x,
                    y: point.y
                })
            }

            nextState.points[1].x += 0.01
            nextState.points[1].y += 0.01
            return nextState
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
    }
}
