import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Commit-11 assembly tests for ExternalCenterlineTripPanel: the five
// commit-10 sub-components wired to the real managers, plus the
// attach/detach completion-signal bridge.
MainWindowTest {
    id: rootId

    property Trip trip: null

    ExternalCenterlineTripPanel {
        id: panelId
        width: 500
        height: 650
        trip: rootId.trip
    }

    ExternalCenterlineTestCase {
        name: "ExternalCenterlineTripPanel"
        when: windowShown

        // cavernOutputChanged dedups identical output, so a reload of an
        // unchanged file may not fire it. markSolved stamps the attached
        // rows on every successful solve — dataChanged is the reliable
        // "a solve completed" observable.
        SignalSpy {
            id: solvedSpyId
            target: RootData.externalCenterlineManager.attachedCenterlinesModel
            signalName: "dataChanged"
        }

        SignalSpy {
            id: attachCompletedSpyId
            target: RootData.externalCenterlineManager
            signalName: "attachCompleted"
        }

        SignalSpy {
            id: detachCompletedSpyId
            target: RootData.externalCenterlineManager
            signalName: "detachCompleted"
        }

        SignalSpy {
            id: stationClickSpyId
            target: panelId
            signalName: "stationClicked"
        }

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            RootData.pageSelectionModel.currentPageAddress = "View"
            rootId.trip = null
            solvedSpyId.clear()
            attachCompletedSpyId.clear()
            detachCompletedSpyId.clear()
            stationClickSpyId.clear()
        }

        function cleanup() {
            RootData.newProject()
        }

        function attachAndBind(projectBaseName) {
            const fixture = attachFixtureTrip(projectBaseName)
            rootId.trip = fixture.trip
            return fixture
        }

        function test_attachRendersSubComponentsInOrder() {
            attachAndBind("trip-panel-attach")

            verify(panelId.isAttached, "panel mode is Attached after the fixture attach")

            compare(attachCompletedSpyId.count, 1, "attach emits exactly one completion")
            const report = attachCompletedSpyId.signalArguments[0][0]
            verify(report.success, "the fixture attach reports success")
            verify(!report.canceled, "a successful attach is not canceled")
            compare(report.errorMessage, "", "no error text on success")
            verify(report.entryFile.indexOf("survex_simple.svx") >= 0,
                   "report carries the persisted entry file; got: " + report.entryFile)
            // QUuid reaches JS as an opaque wrapper — compare() on two of
            // them degenerates to comparing empty JSON. Compare the string
            // forms, pinned to the "{uuid}" shape so the check can't go
            // vacuous through a generic Object toString either.
            verify(String(report.ownerId).indexOf("{") === 0,
                   "ownerId stringifies to a uuid; got: " + String(report.ownerId))
            compare(String(report.ownerId), String(rootId.trip.id))

            const banner = findChild(panelId, "missingSourceBanner")
            const header = findChild(panelId, "attachedHeader")
            const solveStatus = findChild(panelId, "solveStatus")
            const stationsList = findChild(panelId, "stationsList")
            const metadata = findChild(panelId, "tripMetadata")
            verify(banner !== null, "banner must exist")
            verify(!banner.visible, "banner hidden while the source exists")
            verify(header !== null && header.visible, "attached header renders")
            verify(solveStatus !== null && solveStatus.visible, "solve status renders")
            verify(stationsList !== null && stationsList.visible, "stations list renders")
            verify(metadata !== null && metadata.visible, "trip metadata renders")

            const headerY = header.mapToItem(panelId, 0, 0).y
            const solveY = solveStatus.mapToItem(panelId, 0, 0).y
            const stationsY = stationsList.mapToItem(panelId, 0, 0).y
            const metadataY = metadata.mapToItem(panelId, 0, 0).y
            verify(headerY < solveY, "header above solve status")
            verify(solveY < stationsY, "solve status above stations list")
            verify(stationsY < metadataY, "stations list above metadata")

            const fileLabel = findChild(header, "attachedFileLabel")
            verify(fileLabel !== null, "attachedFileLabel must exist")
            tryCompare(fileLabel, "text", "survex_simple.svx")

            // The real per-trip scope prefix ("<caveLabel>.<tripLabel>.")
            // must select the solved stations.
            tryVerify(() => RootData.linePlotManager.lastSolveStationCount > 0,
                      10000, "the attach-chained solve publishes stations")
            tryVerify(() => stationsList.count > 0, 10000,
                      "scoped rows appear for the attached trip")

            // The metadata scan resolves survex_simple.svx (no
            // *calibrate declination) to CaveWhere-owned declination.
            tryVerify(() => !panelId.fileOwnsDeclination, 10000,
                      "fixture without a declination directive is CaveWhere-owned")
            const declEditor = findChild(metadata, "tripMetadataDeclination")
            verify(declEditor !== null, "tripMetadataDeclination must exist")
            tryVerify(() => declEditor.visible, 5000,
                      "declination editor shows when CaveWhere owns declination")
        }

        function test_stationClickForwardsStationHandle() {
            attachAndBind("trip-panel-stations")

            const stationsList = findChild(panelId, "stationsList")
            tryVerify(() => stationsList.count > 0, 10000,
                      "scoped rows appear for the attached trip")

            const listView = findChild(stationsList, "stationsListView")
            tryVerify(() => listView.itemAtIndex(0) !== null, 5000,
                      "first delegate must materialize")

            mouseClick(listView.itemAtIndex(0))
            compare(stationClickSpyId.count, 1, "panel forwards the click")
            const handle = stationClickSpyId.signalArguments[0][0]

            // Identity derivation is the stations list's own test; the panel's
            // job is forwarding, so pin that the handle is the clicked row's.
            compare(handle.tail, listView.itemAtIndex(0).text,
                    "the panel forwards the clicked row's handle")
        }

        function test_reloadRerunsSolve() {
            attachAndBind("trip-panel-reload")
            tryVerify(() => RootData.linePlotManager.lastSolveStationCount > 0,
                      10000, "the attach-chained solve settles first")
            RootData.futureManagerModel.waitForFinished()

            const reloadButton = findChild(panelId, "reloadButton")
            verify(reloadButton !== null, "reloadButton must exist")
            verify(reloadButton.visible && reloadButton.enabled,
                   "Reload is clickable on an idle attached trip")

            solvedSpyId.clear()
            mouseClick(reloadButton)
            tryVerify(() => solvedSpyId.count > 0, 10000,
                      "Reload reruns the solve and restamps the attached rows")
        }

        function test_viewCavernOutputNavigates() {
            attachAndBind("trip-panel-navigate")

            const link = findChild(panelId, "viewCavernOutputLink")
            verify(link !== null, "viewCavernOutputLink must exist")
            mouseClick(link)
            // init() restores "View" before every test.
            tryCompare(RootData.pageSelectionModel, "currentPageAddress", "Cavern")
        }

        function test_detachThroughRemoveAskBox() {
            attachAndBind("trip-panel-detach")
            RootData.futureManagerModel.waitForFinished()

            const detachButton = findChild(panelId, "detachButton")
            verify(detachButton !== null, "detachButton must exist")
            verify(detachButton.visible && detachButton.enabled,
                   "Detach is clickable on an idle attached trip")

            const askBox = findChild(panelId, "detachAskBox")
            verify(askBox !== null, "detachAskBox must exist")
            verify(!askBox.visible, "confirm box hidden until Detach is clicked")

            mouseClick(detachButton)
            tryVerify(() => askBox.visible, 5000, "Detach opens the confirm box")
            verify(askBox.message.indexOf("Detach this trip's centerline?") === 0,
                   "confirm box carries the detach message; got: " + askBox.message)

            const confirmButton = findChild(askBox, "removeButton")
            verify(confirmButton !== null, "confirm button must exist")
            mouseClick(confirmButton)

            // The busy token flips synchronously when the detach starts,
            // so the affordances disable before the future drains.
            compare(panelId.ownerBusy, true, "owner is busy right after confirming")
            const reloadButton = findChild(panelId, "reloadButton")
            verify(!reloadButton.enabled, "Reload disables while the owner is busy")
            verify(!detachButton.enabled, "Detach disables while the owner is busy")

            tryVerify(() => rootId.trip.externalCenterline.entryFile.length === 0,
                      10000, "the trip is Native again after the detach drains")
            tryVerify(() => !panelId.ownerBusy, 10000, "the busy token releases")

            tryVerify(() => detachCompletedSpyId.count === 1, 5000,
                      "detach emits exactly one completion")
            const report = detachCompletedSpyId.signalArguments[0][0]
            verify(report.success, "the detach reports success")
            verify(!panelId.isAttached, "panel leaves Attached mode")
        }

        function test_missingSourceForgetClearsRememberedPath() {
            const fixture = attachAndBind("trip-panel-forget")

            const banner = findChild(panelId, "missingSourceBanner")
            verify(banner !== null, "banner must exist")
            verify(!banner.visible, "banner hidden while the source exists")

            TestHelper.removeFile(TestHelper.toLocalUrl(fixture.source))
            tryVerify(() => banner.visible, 10000,
                      "banner appears once the watcher reports the missing source")

            const forgetButton = findChild(banner, "forgetSourceButton")
            verify(forgetButton !== null, "forgetSourceButton must exist")
            mouseClick(forgetButton)

            tryVerify(() => RootData.externalSourceSettings
                                .sourcePathFor(rootId.trip.id).length === 0,
                      5000, "Forget source clears the remembered path")
            tryVerify(() => !banner.visible, 10000,
                      "banner hides once the owner has no remembered source")
        }

        function test_aSecondAttachmentNothingTiesInBannersItself() {
            const fixture = attachAndBind("trip-panel-floating")
            const cave = RootData.region.cave(0)

            const banner = findChild(panelId, "floatingSurveyBanner")
            verify(banner !== null, "floatingSurveyBanner must exist")

            // Gate on the solve first: "not visible" before any answer has
            // landed is silence, not a negative, and would pass even if the
            // sole attachment in a cave started reporting itself floating.
            tryVerify(() => RootData.linePlotManager.lastSolveStationCount > 0, 10000,
                      "the anchoring attachment solved")
            verify(!banner.visible,
                   "the only attachment in a cave anchors it, so nothing floats")

            // A second centerline in the same cave. The first solved, so it is
            // what the cave is measured against, and no equate joins the second
            // to it — the silent failure this banner exists for.
            //
            // Bound before the attach on purpose: the banner has to appear on
            // the panel the user is already looking at, which is the solve's
            // answer arriving rather than a trip change asking for it.
            cave.addTrip()
            const second = cave.trip(1)
            rootId.trip = second
            verify(!banner.visible, "a trip with no centerline yet floats nothing")

            RootData.attachTripCenterline(second, fixture.source)
            tryVerify(() => banner.visible, 10000,
                      "the untied second attachment banners itself")

            const stations = findChild(banner, "floatingSurveyStations")
            verify(stations !== null, "floatingSurveyStations must exist")
            // Pinned whole rather than searched: a qualified spelling contains
            // the trip-local one as a substring, so indexOf cannot tell the two
            // apart and would pass on exactly the regression this guards.
            compare(stations.text, "Floating: simple.a1, simple.a2, simple.a3",
                    "stations render in the trip's own namespace")

            // The identity behind those strings. The suggester that lands in
            // this banner ties stations, and a tail on its own names no
            // container — so the handles have to reach qml, in step with the
            // strings and pointing at the trip whose scope they live in.
            const status = findChild(panelId, "floatingSurveyStatus")
            verify(status !== null, "floatingSurveyStatus must exist")
            compare(status.stationHandles.length, 3, "one handle per floating station")
            compare(status.stationHandles[0].tail, "simple.a1")
            compare(String(status.stationHandles[0].containerId), String(second.id),
                    "the handle names the trip whose scope the station lives in")

            // Binding back to the anchor must clear it — the banner is about
            // the trip on screen, not about the cave holding a floating one.
            rootId.trip = fixture.trip
            tryVerify(() => !banner.visible, 5000,
                      "the anchoring attachment is not floating")
        }

        function test_aDroppedAttachmentIsNamedAndTiedFromTheBanner() {
            const fixture = attachAndBind("trip-panel-dropped")
            const cave = RootData.region.cave(0)

            const banner = findChild(panelId, "floatingSurveyBanner")
            verify(banner !== null, "floatingSurveyBanner must exist")
            tryVerify(() => RootData.linePlotManager.lastSolveStationCount > 0, 10000,
                      "the anchoring attachment solved")

            // Nothing fixes this second attachment and no equate ties it in, so
            // cavern drops the whole survey rather than placing it in a corner
            // of its own. The Dusk Butte shape, in miniature.
            cave.addTrip()
            const second = cave.trip(1)
            rootId.trip = second
            const dropped = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_hanging_alike.svx")
            RootData.attachTripCenterline(second, dropped)
            tryVerify(() => banner.visible, 10000,
                      "the untied second attachment banners itself")

            // The solve placed none of its stations — the list that shows what
            // it did place stays empty — so every name below came from the scan
            // reading the attachment on its own.
            const stationsList = findChild(panelId, "stationsList")
            verify(stationsList !== null, "stationsList must exist")
            compare(stationsList.count, 0, "a dropped survey has no solved station")

            const stations = findChild(banner, "floatingSurveyStations")
            verify(stations !== null, "floatingSurveyStations must exist")
            tryCompare(stations, "text", "Floating: simple.a1, simple.a2, simple.a3",
                       10000, "harvested stations render in the trip's own namespace")
            verify(stations.visible, "the stations line shows what floats")

            const detail = findChild(banner, "floatingSurveyDetail")
            verify(detail !== null, "floatingSurveyDetail must exist")
            verify(detail.text.indexOf("can't be placed with the others") >= 0,
                   "the copy stops short of claiming they were placed; got: " + detail.text)

            // And the names are worth having because they are tieable: before
            // the harvest this banner listed nothing and offered nothing for
            // exactly the surveys that most needed tying in.
            // Waited for: the suggester runs off the solve this test just
            // kicked, so the row is created some frames after its banner.
            tryVerify(() => findChild(banner, "tieSuggestionText") !== null, 10000,
                      "a suggestion row must render")
            const suggestionText = findChild(banner, "tieSuggestionText")
            tryCompare(suggestionText, "text",
                       "simple.a1 and simple.a1 in " + fixture.trip.name + " are named alike",
                       10000,
                       "the row names the station, its partner, and the trip holding it")

            const connectButton = findChild(banner, "tieSuggestionConnectButton")
            verify(connectButton !== null, "tieSuggestionConnectButton must exist")
            compare(cave.equates.count, 0, "nothing is tied until the user says so")

            mouseClick(connectButton)

            compare(cave.equates.count, 1, "the click records exactly one tie")
            tryVerify(() => !banner.visible, 15000,
                      "the tied attachment is no longer floating")
            tryVerify(() => stationsList.count > 0, 15000,
                      "and the solve now places it, so it has stations of its own")
        }

        function test_aBrokenAttachmentSaysWhichFileIsBroken() {
            const trip = makeSavedTrip("trip-panel-broken")
            rootId.trip = trip

            const errorBanner = findChild(panelId, "externalCenterlineFileErrorBanner")
            verify(errorBanner !== null, "externalCenterlineFileErrorBanner must exist")
            verify(!errorBanner.visible, "nothing is broken before anything is attached")

            const source = TestHelper.testcasesDatasetPath("external-centerlines/broken.svx")
            RootData.attachTripCenterline(trip, source)

            // The region solve fails as one run with one log, so it can say the
            // region is broken but not which attachment broke it. The scan runs
            // cavern over this file alone, which is what makes the complaint
            // land on the panel the user just attached from.
            tryVerify(() => errorBanner.visible, 10000,
                      "the scan's per-file harvest reports the broken attachment")

            const message = findChild(errorBanner, "fileErrorMessage")
            verify(message !== null, "fileErrorMessage must exist")
            verify(message.text.indexOf("broken.svx") >= 0,
                   "cavern's own text names the file; got: " + message.text)

            // And it is the only banner up: a file cavern cannot read fails the
            // region driver as a whole, so the cave solves nothing, no scope is
            // the anchor, and cwFindFloatingSurveys returns before it can name
            // anything adrift. Which is the point — a broken file gets one
            // complaint about the file, not a second one about a float that is
            // really just the first problem seen from the other side.
            const floatingBanner = findChild(panelId, "floatingSurveyBanner")
            verify(floatingBanner !== null, "floatingSurveyBanner must exist")
            verify(!floatingBanner.visible,
                   "a broken file is reported once, as a broken file")
        }

        function test_connectingASuggestionEndsTheFloat() {
            const fixture = attachAndBind("trip-panel-connect")
            const cave = RootData.region.cave(0)

            const banner = findChild(panelId, "floatingSurveyBanner")
            verify(banner !== null, "floatingSurveyBanner must exist")
            tryVerify(() => RootData.linePlotManager.lastSolveStationCount > 0, 10000,
                      "the anchoring attachment solved")

            cave.addTrip()
            const second = cave.trip(1)
            rootId.trip = second
            RootData.attachTripCenterline(second, fixture.source)
            tryVerify(() => banner.visible, 10000,
                      "the untied second attachment banners itself")

            // Both attachments are the same file, so every station in the second
            // is named after one in the first — the ordinary case, and the one
            // the ranking is built for.
            // Waited for: the suggester runs off the solve this test just
            // kicked, so the row is created some frames after its banner.
            tryVerify(() => findChild(banner, "tieSuggestionText") !== null, 10000,
                      "a suggestion row must render")
            const suggestionText = findChild(banner, "tieSuggestionText")
            tryCompare(suggestionText, "text",
                       "simple.a1 and simple.a1 in " + fixture.trip.name + " are named alike",
                       10000,
                       "the row names the station, its partner, and the trip holding it")

            const connectButton = findChild(banner, "tieSuggestionConnectButton")
            verify(connectButton !== null, "tieSuggestionConnectButton must exist")
            compare(cave.equates.count, 0, "nothing is tied until the user says so")

            mouseClick(connectButton)

            // One click is the whole gesture: the equate lands on the cave, the
            // plot re-solves because of it, and the banner that asked for the
            // tie takes itself down. Nothing else on this page has to be touched.
            compare(cave.equates.count, 1, "the click records exactly one tie")
            tryVerify(() => !banner.visible, 15000,
                      "the tied attachment is no longer floating")
        }
    }
}
