import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Commit-11 assembly tests for ExternalCenterlineTripPanel: the five
// commit-10 sub-components wired to the real managers, plus the
// attach completion-signal bridge.
MainWindowTest {
    id: rootId

    property Trip trip: null

    // Tall enough for the whole column; the short-viewport test shrinks the
    // panel and init() puts it back.
    readonly property int fullPanelHeight: 650

    ExternalCenterlineTripPanel {
        id: panelId
        width: 500
        height: rootId.fullPanelHeight
        trip: rootId.trip
    }

    ExternalCenterlineTestCase {
        name: "ExternalCenterlineTripPanel"
        when: windowShown

        SignalSpy {
            id: attachCompletedSpyId
            target: RootData.externalCenterlineManager
            signalName: "attachCompleted"
        }

        SignalSpy {
            id: stationClickSpyId
            target: panelId
            signalName: "stationClicked"
        }

        function init() {
            panelId.height = rootId.fullPanelHeight
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            RootData.pageSelectionModel.currentPageAddress = "View"
            rootId.trip = null
            attachCompletedSpyId.clear()
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

            const header = findChild(panelId, "attachedHeader")
            const solveStatus = findChild(panelId, "solveStatus")
            const stationsList = findChild(panelId, "stationsList")
            const metadata = findChild(panelId, "tripMetadata")
            verify(header !== null && header.visible, "attached header renders")
            verify(header.attached, "the header reads Attached after the fixture attach")
            verify(solveStatus !== null && solveStatus.visible, "solve status renders")
            verify(stationsList !== null && stationsList.visible, "stations list renders")
            verify(metadata !== null && metadata.visible, "trip metadata renders")

            // §16 B2a order: header row, solve status, Date/Declination/
            // Team, then the station list at the bottom.
            waitForRendering(panelId)
            const panelY = (item) => item.mapToItem(panelId, 0, 0).y
            tryVerify(() => panelY(header) < panelY(solveStatus),
                      5000, "header above solve status")
            tryVerify(() => panelY(solveStatus) < panelY(metadata),
                      5000, "solve status above trip metadata")
            tryVerify(() => panelY(metadata) < panelY(stationsList),
                      5000, "trip metadata above the stations list")

            const fileLabel = findChild(header, "attachedFileLabel")
            verify(fileLabel !== null, "attachedFileLabel must exist")
            tryCompare(fileLabel, "text", "survex_simple.svx")

            // The action rides the file-name row, so its vertical center
            // falls inside the file label's band.
            const replaceButton = findChild(header, "replaceButton")
            verify(replaceButton !== null, "replaceButton must exist")
            const centersOnFileRow = () => {
                const labelTop = panelY(fileLabel)
                const labelBottom = labelTop + fileLabel.height
                const center = panelY(replaceButton) + replaceButton.height / 2
                return center >= labelTop && center <= labelBottom
            }
            tryVerify(centersOnFileRow, 5000,
                      "Replace… sits on the file-name row")

            // The Date label leads the metadata block in bold.
            const dateLabel = findChild(metadata, "tripMetadataDateLabel")
            verify(dateLabel !== null, "tripMetadataDateLabel must exist")
            verify(dateLabel.font.bold, "the Date label is bold")

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

        // §16 B2d: a viewport shorter than the content must still reach
        // every block, and it must never scroll sideways.
        function test_aShortPanelScrollsToTheLastBlock() {
            attachAndBind("trip-panel-scroll")

            const stationsList = findChild(panelId, "stationsList")
            verify(stationsList !== null, "stationsList must exist")
            tryVerify(() => stationsList.count > 0, 10000,
                      "scoped rows appear for the attached trip")

            const scrollView = findChild(panelId, "panelScrollView")
            verify(scrollView !== null, "panelScrollView must exist")
            const flickable = scrollView.contentItem

            const shortViewportHeight = 180
            panelId.height = shortViewportHeight
            waitForRendering(panelId)

            tryVerify(() => flickable.contentHeight > flickable.height, 5000,
                      "the short viewport leaves content below the fold; got "
                      + flickable.contentHeight + " in " + flickable.height)

            // The whole width fits, so there is nothing to scroll sideways to.
            tryCompare(flickable, "contentWidth", flickable.width,
                       5000, "the content is exactly the viewport wide")

            // The last block in the B2a order is the station list, and
            // scrolling to the end has to bring it into the viewport.
            // mapToItem(flickable) is already viewport-relative: the
            // flickable's own content item carries the -contentY offset.
            const listBottom = () => stationsList.mapToItem(flickable, 0, 0).y
                    + stationsList.height
            verify(listBottom() > flickable.height,
                   "the stations list starts out below the fold")

            // Fractional layout heights round, so allow a pixel of slack.
            const roundingSlack = 1
            flickable.contentY = flickable.contentHeight - flickable.height
            waitForRendering(panelId)
            tryVerify(() => listBottom() <= flickable.height + roundingSlack, 5000,
                      "scrolling to the end reveals the stations list; bottom at "
                      + listBottom() + " in " + flickable.height)
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

        // plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html §7 q1: the copy is
        // the only file the project reads, so its absence is the state the
        // panel has to name — and Replace is the way out of it.
        function test_aMissingProjectCopyBannersItselfAndOffersReplace() {
            const fixture = attachAndBind("trip-panel-missing-copy")
            tryVerify(() => RootData.linePlotManager.lastSolveStationCount > 0,
                      10000, "the attach-chained solve settles first")
            RootData.futureManagerModel.waitForFinished()

            const banner = findChild(panelId, "missingCenterlineCopyBanner")
            verify(banner !== null, "missingCenterlineCopyBanner must exist")
            verify(!banner.visible, "an attachment whose copy is on disk banners nothing")

            const copyPath = TestHelper.externalCenterlineCopyPath(
                RootData.project, rootId.trip, "survex_simple.svx")
            verify(copyPath.length > 0, "the attachment dir must resolve")
            const copyUrl = TestHelper.toLocalUrl(copyPath)
            verify(TestHelper.fileExists(copyUrl),
                   "the attach copied the file into the project")

            // Deleting the copy fires the watcher, which recomputes.
            TestHelper.removeFile(copyUrl)
            tryVerify(() => banner.visible, 10000,
                      "deleting the in-project copy banners the trip")

            // Named by its project-relative path: the copy is the file the
            // user has now, and the absolute one says nothing they can act on.
            verify(banner.missingPath.length > 0, "the banner names a file")
            verify(banner.missingPath.indexOf("/") !== 0,
                   "the path is project-relative; got: " + banner.missingPath)
            verify(banner.missingPath.indexOf("survex_simple.svx") >= 0,
                   "the path names the missing file; got: " + banner.missingPath)
            const title = findChild(banner, "missingCopyTitle")
            verify(title !== null, "missingCopyTitle must render")
            verify(title.visible && title.font.bold,
                   "the banner leads with its bold title")
            const detail = findChild(banner, "missingCopyDetail")
            verify(detail !== null, "missingCopyDetail must exist")
            verify(detail.text.indexOf(banner.missingPath) >= 0,
                   "the banner text carries the path")

            // The affordance the banner exists for opens the same dialog the
            // actions row does.
            const replaceButton = findChild(banner, "missingCopyReplaceButton")
            verify(replaceButton !== null, "missingCopyReplaceButton must exist")
            verify(findChild(panelId, "replaceCenterlineDialog") === null,
                   "the panel defers the dialog until Replace is clicked")
            waitForRendering(panelId)
            mouseClick(replaceButton)
            const dialog = findChild(panelId, "replaceCenterlineDialog")
            verify(dialog !== null, "the banner's Replace opens the replace dialog")

            // Modal — it has to be off the screen before anything else in the
            // panel is clickable again, and the panel drops it once it is.
            const dialogPopup = findChild(dialog, "replaceDialog")
            verify(dialogPopup !== null, "replaceDialog must exist")
            dialogPopup.close()
            tryVerify(() => findChild(panelId, "replaceCenterlineDialog") === null,
                      5000, "closing the dialog frees it")

            // Putting the file back clears the banner on the next scan.
            verify(TestHelper.copyFile(fixture.source, copyPath),
                   "the fixture copies back into the project")
            RootData.externalCenterlineManager.rescanAttachments()
            tryVerify(() => !banner.visible, 10000,
                      "the restored copy clears the banner")
        }

        // plans/EXTERNAL_FILE_SCAN_STATION_HARVEST_PLAN.html §6: the banner
        // promises Replace is the whole fix, so the missing copy has to cost
        // its own survey and nothing else. Cavern fatals on an *include it
        // cannot open, which would leave every panel in the region reading
        // "Solve failed" instead.
        function test_aMissingProjectCopyLeavesTheSolveStatusCountingStations() {
            attachAndBind("trip-panel-missing-copy-solve")
            tryVerify(() => RootData.linePlotManager.lastSolveStationCount > 0,
                      10000, "the attach-chained solve settles first")

            // A second attachment in the same cave: the survey the region
            // still has to plot once this panel's copy is gone.
            const cave = RootData.region.cave(0)
            cave.addTrip()
            const source = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_simple.svx")
            RootData.attachTripCenterline(cave.trip(1), source)
            tryVerify(() => RootData.externalCenterlineManager
                                .attachedCenterlinesModel.rowCount() === 2,
                      10000, "the second attachment lands its own row")
            RootData.futureManagerModel.waitForFinished()

            const banner = findChild(panelId, "missingCenterlineCopyBanner")
            const solveStatus = findChild(panelId, "solveStatus")
            const statusLabel = findChild(solveStatus, "solveStatusLabel")
            verify(banner !== null, "missingCenterlineCopyBanner must exist")
            verify(statusLabel !== null, "solveStatusLabel must exist")

            const copyPath = TestHelper.externalCenterlineCopyPath(
                RootData.project, rootId.trip, "survex_simple.svx")
            verify(copyPath.length > 0, "the attachment dir must resolve")

            TestHelper.removeFile(TestHelper.toLocalUrl(copyPath))
            tryVerify(() => banner.visible, 10000,
                      "deleting the in-project copy banners the trip")

            // The deletion's own solve is queued through the future manager,
            // so draining it is what makes the status below the region's
            // answer to the missing file rather than the previous one.
            RootData.futureManagerModel.waitForFinished()
            verify(!solveStatus.hasError, "the region still solved")
            verify(solveStatus.stationCount > 0,
                   "the surviving attachment's stations are counted")
            verify(statusLabel.text.indexOf("stations") >= 0,
                   "the status counts stations; got: " + statusLabel.text)
        }

        function test_viewCavernOutputNavigates() {
            attachAndBind("trip-panel-navigate")

            const link = findChild(panelId, "viewCavernOutputLink")
            verify(link !== null, "viewCavernOutputLink must exist")
            mouseClick(link)
            // init() restores "View" before every test.
            tryCompare(RootData.pageSelectionModel, "currentPageAddress", "Cavern")
        }

        // The commit-3b gate (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html
        // §4.1), narrowed by §16 B9b to the one verb that is about the file
        // row. Detaching is now "remove the trip" on the Cave page, so the
        // panel must offer nothing else — enumerated rather than
        // spot-checked, so a re-added action fails here instead of shipping
        // unnoticed.
        function test_attachedPanelOffersReplaceOnly() {
            attachAndBind("trip-panel-actions")
            RootData.futureManagerModel.waitForFinished()

            // §16 B2a parked the actions on the header's file-name row.
            const fileRow = findChild(panelId, "attachedHeaderFileRow")
            verify(fileRow !== null, "attachedHeaderFileRow must exist")

            const rowNames = []
            for (let i = 0; i < fileRow.children.length; ++i) {
                const child = fileRow.children[i]
                if (child.objectName.length > 0) {
                    rowNames.push(child.objectName)
                }
            }
            compare(rowNames.join(","),
                    "attachedFileLabel,attachedFormatLabel,replaceButton",
                    "an attached panel offers Replace… and nothing else")
            // The two labels, the fill spacer and the button — pins the
            // row's size so an action added without an objectName fails here too.
            compare(fileRow.children.length, 4,
                    "the file row holds the name, format, spacer and Replace…")

            // §16 B2e: the panel hands the header an absolute path, so the
            // file-name context menu acts on the copy inside the project.
            const header = findChild(panelId, "attachedHeader")
            verify(header !== null, "attachedHeader must exist")
            verify(RootData.pathExists(header.entryFilePath),
                   "the panel resolves the entry file on disk; got: "
                   + header.entryFilePath)

            const replaceButton = findChild(panelId, "replaceButton")
            verify(replaceButton.visible && replaceButton.enabled,
                   "Replace is clickable on an idle attached trip")

            verify(findChild(panelId, "detachAskBox") === null,
                   "the detach confirm box is gone with the action that opened it")
        }

        function test_replaceSwapsTheAttachedFileThroughTheDialog() {
            attachAndBind("trip-panel-replace")
            RootData.futureManagerModel.waitForFinished()

            const replaceButton = findChild(panelId, "replaceButton")
            verify(replaceButton !== null, "replaceButton must exist")
            verify(replaceButton.visible && replaceButton.enabled,
                   "Replace is clickable on an idle attached trip")

            // The dialog is built by the click that opens it.
            verify(findChild(panelId, "replaceCenterlineDialog") === null,
                   "the panel defers the dialog until Replace is clicked")
            mouseClick(replaceButton)
            const dialog = findChild(panelId, "replaceCenterlineDialog")
            verify(dialog !== null, "replaceCenterlineDialog must exist")

            const pathField = findChild(dialog, "sourcePathField")
            verify(pathField !== null, "sourcePathField must exist")
            const confirmButton = findChild(dialog, "replaceConfirmButton")
            verify(confirmButton !== null, "replaceConfirmButton must exist")
            verify(!confirmButton.enabled, "Replace stays disabled until a file scans")

            // A three-file closure replacing the one-file fixture, so the
            // swap is visible in the entry file the panel shows.
            pathField.text = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_nested.svx")
            tryVerify(() => confirmButton.enabled, 10000,
                      "the scan preview enables Replace")

            attachCompletedSpyId.clear()
            mouseClick(confirmButton)

            // Replace holds the same per-owner token as attach, so the
            // panel's affordances disable while it drains.
            compare(panelId.ownerBusy, true, "owner is busy right after confirming")
            verify(!replaceButton.enabled, "Replace disables while the owner is busy")

            tryVerify(() => attachCompletedSpyId.count === 1, 10000,
                      "replace reports once through the attach bridge")
            const report = attachCompletedSpyId.signalArguments[0][0]
            verify(report.success, "the replace reports success; got: " + report.errorMessage)

            tryVerify(() => rootId.trip.externalCenterline.entryFile === "survex_nested.svx",
                      10000, "the trip now carries the replacement entry file")

            const header = findChild(panelId, "attachedHeader")
            const fileLabel = findChild(header, "attachedFileLabel")
            tryCompare(fileLabel, "text", "survex_nested.svx")
            tryVerify(() => !panelId.ownerBusy, 10000, "the busy token releases")
        }

        // The commit-3 gate (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html
        // §6): whether a source is remembered is no longer something the
        // panel reacts to. Forgetting one is the state change QML can stage
        // — the manager's own test covers a remembered source whose file has
        // since vanished, which needs a setter QML cannot reach.
        function test_forgettingTheSourceLeavesThePanelUnchanged() {
            attachAndBind("trip-panel-forget")

            // Let the relayout the trip binding causes settle before taking
            // the baseline — a snapshot mid-polish would make the exact-y
            // compares below fail for reasons unrelated to forgetting.
            const componentNames = ["attachedHeader", "solveStatus",
                                    "stationsList", "tripMetadata"]
            componentNames.forEach(name => {
                tryVerify(() => {
                    const item = findChild(panelId, name)
                    return item !== null && item.visible
                }, 5000, name + " must exist and be laid out")
            })
            waitForRendering(panelId)
            const before = componentNames.map(name => {
                const item = findChild(panelId, name)
                return { visible: item.visible, y: item.mapToItem(panelId, 0, 0).y }
            })
            verify(RootData.externalSourceSettings
                       .breadcrumbPath(rootId.trip.id).length > 0,
                   "the attach remembered a source path")

            RootData.externalSourceSettings.clearBreadcrumb(rootId.trip.id)
            tryVerify(() => RootData.externalSourceSettings
                                .breadcrumbPath(rootId.trip.id).length === 0,
                      5000, "clearing drops the remembered path")

            // The breadcrumb line is the one thing that may move, and it is
            // inside the header — so wait for it to settle before comparing
            // the layout, or the comparison races the relayout it causes.
            const sourceLabel = findChild(panelId, "sourceModeLabel")
            verify(sourceLabel !== null, "sourceModeLabel must exist")
            tryCompare(sourceLabel, "text",
                       "Copied from an unknown location (this machine)")

            componentNames.forEach((name, i) => {
                const item = findChild(panelId, name)
                compare(item.visible, before[i].visible,
                        name + " keeps its visibility")
                compare(item.mapToItem(panelId, 0, 0).y, before[i].y,
                        name + " keeps its place")
            })
        }

        // §16 B9c: the header stays manager-free, so the panel is what
        // answers the eligibility refresh and what runs the verb. Reload
        // re-copies the source over the in-project copy, which reports
        // through the attach bridge like every other replace.
        function test_sourceMenuReloadRunsTheManagersVerb() {
            attachAndBind("trip-panel-source-reload")
            compare(attachCompletedSpyId.count, 1,
                    "the fixture attach reported once")

            const header = findChild(panelId, "attachedHeader")
            const sourceLabel = findChild(panelId, "sourceModeLabel")
            verify(sourceLabel !== null, "sourceModeLabel must exist")
            const contextMenu = findChild(panelId, "sourceContextMenu")
            verify(contextMenu !== null, "sourceContextMenu must exist")

            waitForRendering(panelId)
            mouseClick(sourceLabel, sourceLabel.width / 2,
                       sourceLabel.height / 2, Qt.RightButton)
            tryVerify(() => contextMenu.menu.opened, 5000,
                      "right-clicking the source line opens the menu")
            tryVerify(() => header.sourceReloadable, 5000,
                      "the panel answers eligibility from the manager")

            const reloadItem = findChild(contextMenu, "reloadFromSourceAction")
            verify(reloadItem !== null, "reloadFromSourceAction must exist")
            verify(reloadItem.enabled, "an existing external source can be reloaded")

            reloadItem.triggered()
            contextMenu.menu.dismiss()
            tryVerify(() => !contextMenu.menu.opened, 5000, "the menu closes")

            tryVerify(() => attachCompletedSpyId.count === 2, 15000,
                      "the reload reports through the attach bridge")
            const report = attachCompletedSpyId.signalArguments[1][0]
            verify(report.success, "the reload succeeds; got: " + report.errorMessage)
            tryVerify(() => !panelId.ownerBusy, 10000, "the busy token releases")
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

            // The solve placed none of its stations, so every name here came
            // from the scan reading the attachment on its own. The list shows
            // them anyway: it lists what the trip is known to have, not what
            // was placed, so it cannot contradict the banner a few pixels below
            // it. Waited for, because the scan's harvest lands well after the
            // attach that provoked it.
            const stationsList = findChild(panelId, "stationsList")
            verify(stationsList !== null, "stationsList must exist")
            tryCompare(stationsList, "count", 3, 10000,
                       "the harvested stations are listed even though none solved")

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
