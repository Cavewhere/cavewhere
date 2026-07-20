import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Commit-13 tests (+ the Add Trip UX plan's unified dialog): the
// CavePage [Add Trip] menu + add-from-survey-file dialog flow
// (orphan-trip cleanup, picked-file naming, attach vs import), the
// TripPage left-pane swap between SurveyEditor and
// ExternalCenterlineTripPanel, the station-reference validator
// relaxation on attached trips, and the carpet sub-page's scoped
// station list.
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

        // Opens the split button's chevron menu on the cave page and
        // returns the requested item.
        function openAddTripMenu(cavePage, itemObjectName) {
            const addBar = findChild(cavePage, "addTrip")
            verify(addBar !== null, "addTrip bar must exist")
            const menuButton = findChild(addBar, "menuButton")
            verify(menuButton !== null, "split-button chevron must exist")
            mouseClick(menuButton)

            const menu = findChild(cavePage, "addTripMenu")
            verify(menu !== null, "addTripMenu must exist")
            tryVerify(() => menu.visible, 5000, "menu opens on chevron click")
            compare(menu.count, 1, "menu shows exactly one item")
            compare(menu.itemAt(0).objectName, "addExternalTripMenuItem")

            for (let i = 0; i < menu.count; ++i) {
                if (menu.itemAt(i).objectName === itemObjectName) {
                    return menu.itemAt(i)
                }
            }
            verify(false, "menu item not found: " + itemObjectName)
            return null
        }

        // Drives the Add trip from survey file… menu item through the
        // dialog with the survex_simple fixture until the requested
        // choice button ("attachButton" or "importButton") is clicked.
        function addTripViaFileMenu(cavePage, choiceButtonName) {
            const externalItem = openAddTripMenu(cavePage, "addExternalTripMenuItem")
            mouseClick(externalItem)

            // Scope to the dialog - ExportImportButtons also has an
            // "importButton" on the cave page.
            const dialog = findChild(cavePage, "attachExternalCenterlineDialog")
            verify(dialog !== null, "the dialog must exist on the cave page")
            const pathField = findChild(dialog, "sourcePathField")
            tryVerify(() => pathField.visible, 5000, "dialog opens")
            pathField.text = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_simple.svx")

            const choiceButton = findChild(dialog, choiceButtonName)
            tryVerify(() => choiceButton.enabled, 10000, "preview scan lands")
            mouseClick(choiceButton)
        }

        function attachViaAddTripMenu(cavePage) {
            addTripViaFileMenu(cavePage, "attachButton")
        }

        function test_addTripPrimaryClickAddsTripWithoutMenu() {
            makeSavedTrip("trip-page-primary-add")
            const cavePage = gotoCavePage()
            compare(currentCave().rowCount(), 1)

            const addBar = findChild(cavePage, "addTrip")
            verify(addBar !== null, "addTrip bar must exist")
            const addButton = findChild(addBar, "addButton")
            verify(addButton !== null, "add button must exist")
            const menu = findChild(cavePage, "addTripMenu")
            verify(menu !== null, "addTripMenu must exist")
            mouseClick(addButton)

            verify(!menu.visible, "primary click never opens the menu")
            tryVerify(() => currentCave().rowCount() === 2, 5000,
                      "primary click adds a trip in one click")
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
            tryVerify(() => pathField.visible, 5000,
                      "Add trip from survey file… opens the dialog")
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

        function test_importViaDialogKeepsNamesAndFillsTrip() {
            makeSavedTrip("trip-page-import-flow")
            const cavePage = gotoCavePage()

            addTripViaFileMenu(cavePage, "importButton")

            // The imported() handler keeps the up-front trip, names it
            // from the picked file, and navigates - same contract as
            // attached(), but the trip stays Native and gets chunks.
            tryVerify(() => currentCave().rowCount() === 2
                            && currentCave().trip(1).chunkCount > 0,
                      10000, "the import lands chunks in the new trip")
            tryCompare(currentCave().trip(1), "name", "survex_simple")
            compare(currentCave().trip(1).externalCenterline.entryFile, "",
                    "an imported trip is Native, not Attached")
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "tripPage",
                      10000, "import navigates to the trip page")
            const tripPage = RootData.pageView.currentPageItem
            tryVerify(() => findChild(tripPage, "surveyEditor") !== null,
                      10000, "an imported trip shows the SurveyEditor")
            RootData.futureManagerModel.waitForFinished()
        }

        // The editor's ListView header is built during a polish pass, so
        // a lookup right after the page opens can answer null. Poll
        // instead of caching that first answer.
        function findWhenReady(item, objectName, message) {
            let found = null
            tryVerify(() => {
                found = findChild(item, objectName)
                return found !== null
            }, 5000, message)
            return found
        }

        // Opens the empty trip page's Add Survey Data chevron menu and
        // returns its Add from survey file… item.
        function openAddSurveyDataMenu(tripPage) {
            const addSurveyData = findWhenReady(
                tripPage, "addSurveyData",
                "the empty state must offer Add Survey Data")
            tryVerify(() => addSurveyData.visible, 5000,
                      "the button shows on an empty trip")
            // The header lays out a frame after the page opens; clicking
            // before that lands on whatever sat at (0, 0).
            waitForRendering(tripPage)

            const menuButton = findWhenReady(tripPage, "menuButton",
                                             "split-button chevron must exist")
            mouseClick(menuButton)

            const menu = findWhenReady(tripPage, "addSurveyDataMenu",
                                       "addSurveyDataMenu must exist")
            tryVerify(() => menu.visible, 5000, "menu opens on chevron click")
            compare(menu.count, 1, "menu shows exactly one item")
            compare(menu.itemAt(0).objectName, "addFromSurveyFileMenuItem")
            return menu.itemAt(0)
        }

        // Opens the dialog from the menu and fills in the fixture path,
        // returning once the scan has landed and the choices are live.
        function armDialogOnTripPage(tripPage) {
            mouseClick(openAddSurveyDataMenu(tripPage))

            const dialog = findWhenReady(tripPage, "attachExternalCenterlineDialog",
                                         "the dialog must exist on the trip page")
            const pathField = findChild(dialog, "sourcePathField")
            tryVerify(() => pathField.visible, 5000, "dialog opens")
            pathField.text = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_simple.svx")

            tryVerify(() => findChild(dialog, "attachButton").enabled, 10000,
                      "preview scan lands")
            return dialog
        }

        // Drives an armed dialog through the requested choice button.
        function addFromFileOnTripPage(tripPage, choiceButtonName) {
            const dialog = armDialogOnTripPage(tripPage)

            const choiceButton = findChild(dialog, choiceButtonName)
            tryVerify(() => choiceButton.enabled, 10000, "the choice is live")
            // The scan result swaps the labels above the choice boxes,
            // which shifts them; settle before aiming at one.
            waitForRendering(tripPage)
            mouseClick(choiceButton)
            return dialog
        }

        function test_tripPageAttachSwapsPaneWithoutRenamingTrip() {
            const trip = makeSavedTrip("trip-page-add-from-file-attach")
            const originalName = trip.name
            const tripPage = gotoTripPage(trip)

            addFromFileOnTripPage(tripPage, "attachButton")

            tryVerify(() => trip.externalCenterline.entryFile === "survex_simple.svx",
                      10000, "the existing trip becomes Attached")
            tryVerify(() => findChild(tripPage, "externalCenterlineTripPanel") !== null,
                      10000, "the left pane swaps to ExternalCenterlineTripPanel")
            compare(currentCave().rowCount(), 1, "no trip is created or removed")
            compare(trip.name, originalName, "attaching never renames an existing trip")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_tripPageImportFillsSameTripWithoutRenaming() {
            const trip = makeSavedTrip("trip-page-add-from-file-import")
            const originalName = trip.name
            const tripPage = gotoTripPage(trip)

            addFromFileOnTripPage(tripPage, "importButton")

            tryVerify(() => trip.chunkCount > 0, 10000,
                      "the import lands chunks in the same trip")
            compare(currentCave().rowCount(), 1, "no trip is created or removed")
            compare(trip.name, originalName, "importing never renames an existing trip")
            compare(trip.externalCenterline.entryFile, "",
                    "an imported trip stays Native")
            verify(findChild(tripPage, "surveyEditor") !== null,
                   "the editor keeps showing the now-filled trip")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_tripPageCancelLeavesTripNativeAndEmpty() {
            const trip = makeSavedTrip("trip-page-add-from-file-cancel")
            const tripPage = gotoTripPage(trip)

            // Cancel a fully armed dialog - the state where the attach
            // and import wiring could still fire on the way out.
            const dialog = armDialogOnTripPage(tripPage)
            const pathField = findChild(dialog, "sourcePathField")
            waitForRendering(tripPage)
            mouseClick(findChild(dialog, "cancelButton"))

            tryVerify(() => !pathField.visible, 5000, "Cancel closes the dialog")
            compare(currentCave().rowCount(), 1, "cancel has no trip to unwind")
            compare(trip.chunkCount, 0, "the trip is still empty")
            compare(trip.externalCenterline.entryFile, "", "the trip is still Native")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_addSurveyDataSplitButtonHiddenOnceTripHasData() {
            const trip = makeSavedTrip("trip-page-add-from-file-nonempty")
            const tripPage = gotoTripPage(trip)

            const menuButton = findWhenReady(tripPage, "menuButton",
                                             "the chevron must exist")
            tryVerify(() => menuButton.visible, 5000,
                      "the split button shows while the trip is empty")

            trip.addNewChunk()
            tryVerify(() => !menuButton.visible, 5000,
                      "the whole split button is empty-state only")
            RootData.futureManagerModel.waitForFinished()
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
