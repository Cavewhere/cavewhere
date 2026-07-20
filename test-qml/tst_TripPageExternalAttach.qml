import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Commit-13 tests: the CavePage [Add Trip] menu + attach-dialog flow
// (orphan-trip cleanup, entry-file naming), the TripPage left-pane
// swap between SurveyEditor and ExternalCenterlineTripPanel, the
// station-reference validator relaxation on attached trips, and the
// carpet sub-page's scoped station list.
MainWindowTest {
    id: rootId

    ExternalCenterlineTestCase {
        name: "TripPageExternalAttach"
        when: windowShown

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function cleanup() {
            RootData.newProject()
        }

        function currentCave() {
            return RootData.region.cave(0)
        }

        function gotoCavePage() {
            RootData.pageSelectionModel.currentPageAddress =
                "Source/Data/Cave=" + currentCave().name
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      10000, "cave page opens")
            // The action bar is placed via LayoutItemProxy; clicks miss
            // until the proxied layout settles.
            waitForRendering(rootId)
            return RootData.pageView.currentPageItem
        }

        function gotoTripPage(trip) {
            RootData.pageSelectionModel.currentPageAddress =
                "Source/Data/Cave=" + currentCave().name + "/Trip=" + trip.name
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "tripPage",
                      10000, "trip page opens")
            return RootData.pageView.currentPageItem
        }

        // Opens the two-item Add Trip menu on the cave page and returns
        // the requested item.
        function openAddTripMenu(cavePage, itemObjectName) {
            const addBar = findChild(cavePage, "addTrip")
            verify(addBar !== null, "addTrip bar must exist")
            const addButton = findChild(addBar, "addButton")
            verify(addButton !== null, "add button must exist")
            mouseClick(addButton)

            const menu = findChild(cavePage, "addTripMenu")
            verify(menu !== null, "addTripMenu must exist")
            tryVerify(() => menu.visible, 5000, "menu opens on click")
            compare(menu.count, 2, "menu shows exactly two items")
            compare(menu.itemAt(0).objectName, "addNativeTripMenuItem")
            compare(menu.itemAt(1).objectName, "addExternalTripMenuItem")

            for (let i = 0; i < menu.count; ++i) {
                if (menu.itemAt(i).objectName === itemObjectName) {
                    return menu.itemAt(i)
                }
            }
            verify(false, "menu item not found: " + itemObjectName)
            return null
        }

        // Drives the External centerline… menu item through the dialog
        // with the survex_simple fixture until the attach is submitted.
        function attachViaAddTripMenu(cavePage) {
            const externalItem = openAddTripMenu(cavePage, "addExternalTripMenuItem")
            mouseClick(externalItem)

            const pathField = findChild(cavePage, "sourcePathField")
            verify(pathField !== null, "attach dialog must exist on the cave page")
            tryVerify(() => pathField.visible, 5000, "dialog opens")
            pathField.text = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_simple.svx")

            const attachButton = findChild(cavePage, "attachButton")
            tryVerify(() => attachButton.enabled, 10000, "preview scan lands")
            mouseClick(attachButton)
        }

        function test_addTripMenuNativeItemAddsTrip() {
            makeSavedTrip("trip-page-menu-native")
            const cavePage = gotoCavePage()
            compare(currentCave().rowCount(), 1)

            const nativeItem = openAddTripMenu(cavePage, "addNativeTripMenuItem")
            mouseClick(nativeItem)

            tryVerify(() => currentCave().rowCount() === 2, 5000,
                      "Native trip adds a trip like the old plain button")
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "tripPage",
                      10000, "and navigates to the new trip")
            compare(currentCave().trip(1).externalCenterline.entryFile, "",
                    "the new trip is Native")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_externalMenuItemOpensDialogAndCancelRemovesOrphan() {
            makeSavedTrip("trip-page-menu-cancel")
            const cavePage = gotoCavePage()
            compare(currentCave().rowCount(), 1)

            const externalItem = openAddTripMenu(cavePage, "addExternalTripMenuItem")
            mouseClick(externalItem)

            const pathField = findChild(cavePage, "sourcePathField")
            verify(pathField !== null, "attach dialog must exist on the cave page")
            tryVerify(() => pathField.visible, 5000, "External centerline… opens the dialog")
            compare(currentCave().rowCount(), 2, "the flow creates the trip up front")

            // Scope to the dialog - CavePage's RemoveAskBox also has a
            // "cancelButton".
            const dialog = findChild(cavePage, "attachExternalCenterlineDialog")
            verify(dialog !== null, "dialog root must exist")
            const cancelButton = findChild(dialog, "cancelButton")
            verify(cancelButton !== null, "dialog cancel button must exist")
            mouseClick(cancelButton)
            tryVerify(() => !pathField.visible, 5000, "Cancel closes the dialog")
            tryVerify(() => currentCave().rowCount() === 1, 5000,
                      "the orphan trip is removed on dismissal")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_attachViaDialogNamesTripAndSwapsPane() {
            makeSavedTrip("trip-page-attach-flow")
            const cavePage = gotoCavePage()

            attachViaAddTripMenu(cavePage)

            tryVerify(() => currentCave().rowCount() === 2
                            && currentCave().trip(1).externalCenterline.entryFile
                               === "survex_simple.svx",
                      10000, "the new trip becomes Attached")
            tryCompare(currentCave().trip(1), "name", "survex_simple")

            // The attached() handler navigates to the new trip's page,
            // where the swap shows the panel instead of the editor.
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "tripPage",
                      10000, "attach navigates to the trip page")
            const tripPage = RootData.pageView.currentPageItem
            tryVerify(() => findChild(tripPage, "externalCenterlineTripPanel") !== null,
                      10000, "the left pane swaps to ExternalCenterlineTripPanel")
            verify(findChild(tripPage, "surveyEditor") === null,
                   "SurveyEditor is not instantiated for an attached trip")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_attachSameFileTwiceDeduplicatesTripName() {
            makeSavedTrip("trip-page-dup-name")

            for (let i = 1; i <= 2; ++i) {
                const cavePage = gotoCavePage()
                attachViaAddTripMenu(cavePage)
                tryVerify(() => currentCave().rowCount() === 1 + i
                                && RootData.pageView.currentPageItem.objectName
                                   === "tripPage",
                          10000, "attach " + i + " lands and navigates")
                RootData.futureManagerModel.waitForFinished()
            }

            // cwCave::uniqueTripName dedupes the second rename; a raw
            // setName would silently no-op on the collision.
            tryCompare(currentCave().trip(1), "name", "survex_simple")
            tryCompare(currentCave().trip(2), "name", "survex_simple 2")
        }

        function test_detachSwapsPaneBackToSurveyEditor() {
            const fixture = attachFixtureTrip("trip-page-detach-swap")
            const tripPage = gotoTripPage(fixture.trip)

            tryVerify(() => findChild(tripPage, "externalCenterlineTripPanel") !== null,
                      10000, "attached trip shows the panel")
            verify(findChild(tripPage, "surveyEditor") === null,
                   "no SurveyEditor while attached")
            RootData.futureManagerModel.waitForFinished()

            const detachButton = findChild(tripPage, "detachButton")
            verify(detachButton !== null, "detach button must exist")
            tryVerify(() => detachButton.enabled, 10000, "owner idles before detach")
            mouseClick(detachButton)

            const askBox = findChild(tripPage, "detachAskBox")
            tryVerify(() => askBox.visible, 5000, "confirm box opens")
            mouseClick(findChild(askBox, "removeButton"))

            tryVerify(() => fixture.trip.externalCenterline.entryFile.length === 0,
                      10000, "the trip is Native again")
            tryVerify(() => findChild(tripPage, "surveyEditor") !== null,
                      10000, "the left pane swaps back to SurveyEditor")
            // The Loader tears the old item down via deleteLater; give
            // the queued deletion a turn.
            tryVerify(() => findChild(tripPage, "externalCenterlineTripPanel") === null,
                      5000, "the panel unloads after detach")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_stationValidatorFollowsTripAttachment() {
            // A real project with a note + scrap, so the validator is
            // checked on the production NoteStation item, not a harness.
            TestHelper.loadProjectFromFile(
                RootData.project,
                TestHelper.testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"))
            RootData.futureManagerModel.waitForFinished()

            // The attach orchestrator refuses temporary projects.
            const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tmpPath + "/trip-page-validator.cwproj"),
                   "saveAs should succeed")
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            const trip = currentCave().trip(0)
            const tripPage = gotoTripPage(trip)

            // Show the note in carpet mode so the scrap station items
            // materialize.
            const carpetButton = ObjectFinder.findObjectByChain(
                rootId.mainWindow, "rootId->tripPage->noteGallery->mainButtonArea->carpetButtonId")
            verify(carpetButton !== null, "carpet button must exist")
            mouseClick(carpetButton)

            const noteArea = ObjectFinder.findObjectByChain(
                rootId.mainWindow, "rootId->tripPage->noteGallery->noteArea")
            verify(noteArea !== null, "note area must exist")
            tryVerify(() => noteArea.note !== null, 10000, "the note loads")

            verify(TestHelper.addScrapStation(noteArea.note, 0, "vt1", Qt.point(0.5, 0.5)),
                   "seeding a station on the existing scrap succeeds")

            let stationItem = null
            tryVerify(() => {
                stationItem = findChild(noteArea, "stationvt1")
                return stationItem !== null
            }, 10000, "the station item materializes")

            const validator = findChild(stationItem, "stationValidator")
            verify(validator !== null, "the station input has a validator")

            // QValidator::Acceptable === 2.
            compare(validator.external, false, "native trip keeps the strict grammar")
            verify(validator.validate("A.1") !== 2, "dotted name rejected on a Native trip")
            compare(validator.validate("A1"), 2, "plain name accepted on a Native trip")

            RootData.attachTripCenterline(
                trip,
                TestHelper.testcasesDatasetPath("external-centerlines/survex_simple.svx"))
            tryVerify(() => validator.external, 10000,
                      "the validator relaxes when the trip attaches")
            compare(validator.validate("A.1"), 2, "dotted name accepted on an Attached trip")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_carpetSubPageListsScopeStations() {
            const fixture = attachFixtureTrip("trip-page-carpet")
            let tripPage = gotoTripPage(fixture.trip)

            tryVerify(() => findChild(tripPage, "externalCenterlineTripPanel") !== null,
                      10000, "attached trip shows the panel")

            // Visiting the trip page registers the Carpet sub-page.
            RootData.pageSelectionModel.currentPageAddress =
                "Source/Data/Cave=" + currentCave().name + "/Trip=" + fixture.trip.name
                + "/Carpet"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "tripPage",
                      10000, "carpet sub-page opens on the trip page item")
            tripPage = RootData.pageView.currentPageItem
            tryCompare(tripPage, "viewMode", "CARPET")
            tryCompare(tripPage, "state", "COLLAPSE")

            // Expand the collapsed left pane; for an attached trip it is
            // the panel, whose station list is the carpet-mode picker.
            const expandButton = findChild(tripPage, "expandLeftPaneButton")
            verify(expandButton !== null, "expand button must exist")
            mouseClick(expandButton)
            tryCompare(tripPage, "state", "")

            const stationsList = findChild(tripPage, "stationsList")
            verify(stationsList !== null, "panel stations list must exist")
            tryVerify(() => stationsList.visible, 5000,
                      "stations list shows after expanding")

            // survex_simple.svx solves to exactly A1/A2/A3. The rows come
            // from cwScopeStationListModel's prefix-scoped slice of the
            // post-solve network - the attached trip has no native shots
            // (chunkCount 0), so cwTrip::stations() could never produce
            // them.
            tryVerify(() => stationsList.count === 3, 10000,
                      "carpet picker lists the scoped stations; got: "
                      + stationsList.count)
            compare(fixture.trip.chunkCount, 0,
                    "the attached trip has no native survey chunks")
            RootData.futureManagerModel.waitForFinished()
        }
    }
}
