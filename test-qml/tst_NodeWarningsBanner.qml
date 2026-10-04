import QtQuick
import QtQuick.Controls as QC
import QtTest
import cavewherelib
import cw.TestLib

// NodeWarningsBanner on CavePage: one line per warning the cave carries, and a
// tap on a line opens where the warning is fixed.
MainWindowTest {
    id: rootId

    // Shared by both cases below: the page's banner and its lines.
    component BannerTestCase: ExternalCenterlineTestCase {
        function bannerOn(page) {
            return findChild(page, "nodeWarningsBanner")
        }

        // The banner's lines, in order.
        function entries(bannerItem) {
            const found = []
            for (let i = 0; ; ++i) {
                const entry = findChild(bannerItem, "nodeWarning." + i)
                if (entry === null) {
                    return found
                }
                found.push(entry)
            }
        }

        function messageLabel(entry) {
            return findChild(entry, "nodeWarningMessage")
        }

        function detailLabel(entry) {
            return findChild(entry, "nodeWarningDetail")
        }

        // The entry whose message contains `fragment`, or null.
        function entryContaining(bannerItem, fragment) {
            const all = entries(bannerItem)
            for (let i = 0; i < all.length; ++i) {
                if (messageLabel(all[i]).text.indexOf(fragment) >= 0) {
                    return all[i]
                }
            }
            return null
        }

        function bannerText(bannerItem) {
            return entries(bannerItem).map(entry => messageLabel(entry).text).join("\n")
        }
    }

    BannerTestCase {
        name: "NodeWarningsBannerFixStations"
        when: windowShown

        property Cave cave: null

        function gotoCavePage() {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=OutlierCave"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "cavePage")
        }

        function initTestCase() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.region.addCave()
            cave = RootData.region.cave(0)
            cave.name = "OutlierCave"
            gotoCavePage()
        }

        function init() {
            gotoCavePage()
            // Drain fix stations between cases so each starts with no outlier.
            while (cave.fixStations.count > 0) {
                cave.fixStations.removeAt(0)
            }
        }

        function banner() {
            const b = bannerOn(RootData.pageView.currentPageItem)
            verify(b !== null, "the warnings banner must exist in CavePage")
            return b
        }

        function fixStationsBadge() {
            return findChild(RootData.pageView.currentPageItem, "fixStationsBadge")
        }

        function bannerProxy() {
            return findChild(RootData.pageView.currentPageItem, "nodeWarningsBannerProxy")
        }

        function addUtm13NFix(name, e, n, z) {
            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(cave.fixStations.count - 1)
            cave.fixStations.setData(idx, name, FixStationModel.StationNameRole)
            cave.fixStations.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            cave.fixStations.setData(idx, e, FixStationModel.EastingRole)
            cave.fixStations.setData(idx, n, FixStationModel.NorthingRole)
            cave.fixStations.setData(idx, z, FixStationModel.ElevationRole)
        }

        // A tight cluster of four good fixes near Boulder, UTM Z13N. The cluster
        // rule needs at least three fixes to have a majority to judge an outlier
        // against; four keeps a clear majority once a straggler is added.
        function addGoodCluster() {
            addUtm13NFix("g1", 478000.0, 4430000.0, 1655.0)
            addUtm13NFix("g2", 478010.0, 4430010.0, 1656.0)
            addUtm13NFix("g3", 477990.0, 4429990.0, 1654.0)
            addUtm13NFix("g4", 478005.0, 4430005.0, 1655.0)
        }

        // One good fix, so the project has a frame. A domain-invalid fix can't
        // anchor one, and the distance check runs only once there is a frame.
        function addAnchorFix() {
            addUtm13NFix("anchor", 478000.0, 4430000.0, 1655.0)
        }

        // ── No outlier → banner hidden ───────────────────────────────────────

        function test_bannerHiddenWithoutOutlier() {
            const b = banner()
            verify(!b.visible, "banner is hidden with no fix stations")

            addGoodCluster()
            verify(!b.visible, "a clean cluster raises no warning")
        }

        // The proxy that hosts the banner in the wide layout must be hidden when
        // there is no warning — a visible proxy with an invisible target still
        // reserves an empty full-width slot (an empty "badge") under the cave
        // name. An invisible layout item is excluded from the layout.
        function test_bannerProxyHiddenWithoutOutlier() {
            const proxy = bannerProxy()
            verify(proxy !== null, "banner proxy must exist in the wide layout")
            verify(!proxy.visible, "empty banner proxy must be hidden")

            addGoodCluster()
            verify(!proxy.visible, "a clean cluster keeps the proxy hidden")

            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)
            tryVerify(() => proxy.visible, 1000,
                      "proxy appears once a warning does")
        }

        // ── Typo'd fix → banner names the station and how far off it is ──────

        function test_bannerAppearsForOutlier() {
            addGoodCluster()
            // A transposed leading digit (1478000 easting) falls outside UTM 13N's
            // valid domain, so the per-fix domain check flags it on its own.
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)

            const b = banner()
            tryVerify(() => b.visible, 1000, "banner appears once a fix is an outlier")
            compare(entries(b).length, 1, "one line for the one warning")
            const text = bannerText(b)
            verify(text.indexOf("BAD") >= 0,
                   "banner names the offending station: " + text)
            verify(text.indexOf("outside the valid range") >= 0,
                   "banner explains the problem: " + text)
        }

        // ── The banner renders station names, it doesn't interpret them ─────

        // The messages quote whatever the user typed into the station-name
        // field. A station named A<b>B must reach the screen intact instead of
        // being swallowed as a tag.
        function test_bannerRendersStationNameLiterally() {
            addAnchorFix()
            addUtm13NFix("A<b>B", 1478000.0, 4430000.0, 1655.0)

            const b = banner()
            tryVerify(() => b.visible, 1000, "banner appears for the bad fix")
            const label = messageLabel(entries(b)[0])
            compare(label.textFormat, QC.Label.PlainText,
                    "warning text must not be read as markup")
            verify(label.text.indexOf("A<b>B") >= 0,
                   "banner quotes the station name verbatim: " + label.text)
        }

        // ── Correcting the coordinate clears the banner ─────────────────────

        function test_bannerClearsWhenCoordinateCorrected() {
            addGoodCluster()
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)

            const b = banner()
            tryVerify(() => b.visible, 1000, "banner is up before the correction")

            // Move the typo'd fix back into the cluster.
            const idx = cave.fixStations.index(cave.fixStations.count - 1)
            cave.fixStations.setData(idx, 478000.0, FixStationModel.EastingRole)

            tryVerify(() => !b.visible, 1000,
                      "banner clears once the coordinate is corrected")
        }

        // ── Suppressing the warning hides the banner ────────────────────────

        function test_bannerHidesWhenWarningSuppressed() {
            addGoodCluster()
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)

            const b = banner()
            tryVerify(() => b.visible, 1000, "banner is up before suppression")

            const errors = cave.errorModel.errors
            compare(errors.count, 1, "one cave-level warning is present")
            errors.setData(errors.index(0, 0), true, ErrorListModel.SuppressedRole)

            tryVerify(() => !b.visible, 1000,
                      "banner hides once the warning is suppressed")
        }

        // ── A fix-station line opens the Fix Stations page on that fix ──────

        function test_fixStationEntryOpensItsRow() {
            addGoodCluster()
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)
            const badRow = cave.fixStations.count - 1

            const b = banner()
            tryVerify(() => b.visible, 1000, "banner is up for the bad fix")
            waitForRendering(rootId)

            mouseClick(messageLabel(entries(b)[0]))

            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "the line opens the cave's Fix Stations page")
            const tableView = findChild(RootData.pageView.currentPageItem, "fixStationTableView")
            verify(tableView !== null, "fixStationTableView must exist")
            tryCompare(tableView, "currentIndex", badRow,
                       5000, "the page selects the fix the warning names")
        }

        // The same line works again after the user has picked another row.
        function test_fixStationEntryReselectsAfterUserMovesOn() {
            addGoodCluster()
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)
            const badRow = cave.fixStations.count - 1

            let b = banner()
            tryVerify(() => b.visible, 1000, "banner is up for the bad fix")
            waitForRendering(rootId)
            mouseClick(messageLabel(entries(b)[0]))
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "fixStationPage")
            const tableView = findChild(RootData.pageView.currentPageItem, "fixStationTableView")
            tryCompare(tableView, "currentIndex", badRow)

            RootData.pageView.currentPageItem.pickRow(0)
            compare(tableView.currentIndex, 0)

            gotoCavePage()
            b = banner()
            tryVerify(() => b.visible, 1000, "banner is still up")
            waitForRendering(rootId)
            mouseClick(messageLabel(entries(b)[0]))
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "fixStationPage")
            tryCompare(tableView, "currentIndex", badRow,
                       5000, "the line selects its fix again")
        }

        // ── The "Fix stations:" line badge (U9) — a finer, scoped indicator ──

        function test_fixStationBadgeHiddenWithoutError() {
            const badge = fixStationsBadge()
            verify(badge !== null, "fix-station badge must exist on CavePage")
            verify(!badge.visible, "badge hidden with no fix-station error")

            addGoodCluster()
            verify(!badge.visible, "a clean cluster raises no badge")
        }

        function test_fixStationBadgeAppearsForError() {
            const badge = fixStationsBadge()
            // A single out-of-domain fix (Part A) is enough to flag the cave.
            addAnchorFix()
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)
            tryVerify(() => badge.visible, 1000, "badge appears once a fix errors")

            // Correcting the coordinate clears the badge.
            const idx = cave.fixStations.index(cave.fixStations.count - 1)
            cave.fixStations.setData(idx, 478000.0, FixStationModel.EastingRole)
            tryVerify(() => !badge.visible, 1000, "badge clears once corrected")
        }

        function test_fixStationBadgeHidesWhenSuppressed() {
            addAnchorFix()
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)
            const badge = fixStationsBadge()
            tryVerify(() => badge.visible, 1000, "badge up before suppression")

            const errors = cave.errorModel.errors
            tryCompare(errors, "count", 1)
            errors.setData(errors.index(0, 0), true, ErrorListModel.SuppressedRole)
            tryVerify(() => !badge.visible, 1000, "badge hides once suppressed")
        }
    }

    BannerTestCase {
        name: "NodeWarningsBannerSolve"
        when: windowShown

        function init() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        // Off the trip page first, so the page never sees its trip deleted.
        function cleanup() {
            if (restoreWidth > 0) {
                rootId.width = restoreWidth
                restoreWidth = 0
            }
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.project.newProject()
        }

        function gotoCavePage(cave) {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + cave.name
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "cavePage",
                      10000, "the cave page opens")
            waitForRendering(rootId)
            return RootData.pageView.currentPageItem
        }

        // A trip of one shot, `from` to `to`.
        function addTripWithShot(cave, tripName, from, to) {
            cave.addTrip()
            const trip = cave.trip(cave.tripCount - 1)
            trip.name = tripName
            trip.addNewChunk()
            const chunk = trip.chunk(0)
            chunk.setData(SurveyChunk.StationNameRole, 0, from)
            chunk.setData(SurveyChunk.StationNameRole, 1, to)
            chunk.setData(SurveyChunk.ShotDistanceRole, 0, "10")
            chunk.setData(SurveyChunk.ShotCompassRole, 0, "0")
            chunk.setData(SurveyChunk.ShotClinoRole, 0, "0")
            return trip
        }

        // The window's width before a test narrowed it, or 0 while it is wide.
        property int restoreWidth: 0

        SignalSpy {
            id: sourceLineSpyId
            signalName: "sourceLineRequested"
        }

        // Expects the source line's summary to pulse once and settle.
        function verifyPulses(summary) {
            tryVerify(() => summary.attentionActive, 5000, "the source line pulses")
            tryVerify(() => !summary.attentionActive, 5000, "the pulse settles")
        }

        // Expects the page's banner to raise sourceLineRequested for `entry`
        // and leave the attached file's source line on screen, pulsing, in the
        // wide layout and then the narrow one.
        function verifyOpensSourceLine(cavePage, bannerItem, entry) {
            const summary = findChild(cavePage, "externalCaveSummary")
            verify(summary !== null)
            tryVerify(() => !summary.attentionActive, 5000, "the source line starts at rest")

            sourceLineSpyId.target = bannerItem
            sourceLineSpyId.clear()
            mouseClick(messageLabel(entry))
            compare(sourceLineSpyId.count, 1, "the line asks for the source line")
            compare(RootData.pageView.currentPageItem, cavePage,
                    "the source line is on the cave page itself")
            verifyPulses(summary)

            // The Flickable carries no objectName of its own: one would sit in
            // every "rootId->cavePage->..." chain the other tests walk.
            const flickable = findChild(cavePage, "cavePageVerticalScrollBar").parent
            verify(flickable !== null)
            const wideColumn = summary.parent
            const top = summary.mapToItem(flickable, 0, 0).y
            verify(top >= 0 && top < flickable.height,
                   "the source line is in view: " + top)

            // The summary starts in view on this page, so scroll it away and
            // ask again to see the page bring it back. The banner scrolls away
            // with it, so the request is raised directly.
            flickable.contentY = summary.mapToItem(flickable.contentItem, 0, 0).y + flickable.height
            verify(summary.mapToItem(flickable, 0, 0).y < 0, "the source line scrolled away")
            bannerItem.sourceLineRequested()
            const backTop = summary.mapToItem(flickable, 0, 0).y
            verify(backTop >= 0 && backTop < flickable.height,
                   "the page scrolls the source line back into view: " + backTop)
            verifyPulses(summary)

            // The narrow column shows the summary under the stats, so the
            // line pulses it where it stands.
            restoreWidth = rootId.width
            rootId.width = Math.round(Theme.breakpointPanelCollapse * 2 / 3)
            tryVerify(() => cavePage.isNarrow, 5000, "the page takes its narrow layout")
            waitForRendering(rootId)
            const narrowEntry = entryContaining(bannerItem, messageLabel(entry).text)
            verify(narrowEntry !== null, "the narrow banner lists the line")
            sourceLineSpyId.clear()
            mouseClick(messageLabel(narrowEntry))
            compare(sourceLineSpyId.count, 1, "the narrow line asks for the source line")
            verify(summary.visible, "the narrow column shows the source line")
            // The proxy moves the summary into the narrow column, which fills
            // the page with no scrolling of its own: it is the viewport.
            const narrowColumn = summary.parent
            verify(narrowColumn !== wideColumn, "the summary sits in the narrow column")
            const narrowTop = summary.mapToItem(narrowColumn, 0, 0).y
            verify(narrowTop >= 0 && narrowTop < narrowColumn.height,
                   "the narrow source line is in view: " + narrowTop)
            const columnTop = narrowColumn.mapToItem(cavePage, 0, 0).y
            verify(columnTop >= 0 && columnTop + narrowColumn.height <= cavePage.height,
                   "the narrow column fits the page: " + columnTop)
            verifyPulses(summary)
        }

        // ── An untied survey in a cave-level attach ─────────────────────────

        function test_unconnectedAttachEntryOpensSourceLine() {
            const cave = makeSavedCaveAttach("node-warnings-untied",
                                             "external-centerlines/compass_untied.dat")
            const cavePage = gotoCavePage(cave)
            const bannerItem = bannerOn(cavePage)
            verify(bannerItem !== null)

            tryVerify(() => bannerItem.visible
                      && entryContaining(bannerItem, "not tied") !== null,
                      20000, "the solve's untied survey reaches the banner")
            const entry = entryContaining(bannerItem, "not tied")
            compare(messageLabel(entry).text,
                    "3 stations in compass_untied.dat are not tied to the cave")
            const detail = detailLabel(entry)
            verify(detail.visible, "the station names show under the message")
            compare(detail.text, "u1, u2, u3")

            verifyOpensSourceLine(cavePage, bannerItem, entry)
        }

        // ── An untied native trip ───────────────────────────────────────────

        // The solve writes a native trip's untied stations on the trip, so the
        // cave's banner lists the trip's warning and opens the trip's page.
        function test_unconnectedNativeTripEntryOpensTrip() {
            RootData.region.addCave()
            const cave = RootData.region.cave(0)
            cave.name = "NativeCave"
            addTripWithShot(cave, "Tied", "a1", "a2")
            const untied = addTripWithShot(cave, "Untied", "b1", "b2")

            const cavePage = gotoCavePage(cave)
            const bannerItem = bannerOn(cavePage)
            tryVerify(() => bannerItem.visible
                      && entryContaining(bannerItem, "not tied") !== null,
                      20000, "the untied trip reaches the cave's banner")
            const entry = entryContaining(bannerItem, "not tied")
            verify(messageLabel(entry).text.indexOf("Untied") >= 0,
                   "the line names the untied trip: " + messageLabel(entry).text)

            mouseClick(messageLabel(entry))
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "tripPage",
                      5000, "the line opens a trip page")
            compare(RootData.pageView.currentPageItem.currentTrip, untied,
                    "the page is the untied trip's")
        }

        // A native cave with a fix in a real system gives the project its
        // frame, which is what turns an attached file's fixes into warnings.
        function addFixedBesideCave() {
            RootData.region.addCave()
            const beside = RootData.region.cave(RootData.region.rowCount() - 1)
            beside.name = "Beside"
            addTripWithShot(beside, "Native", "b1", "b2")
            beside.fixStations.addFixStation()
            const idx = beside.fixStations.index(0)
            beside.fixStations.setData(idx, "b1", FixStationModel.StationNameRole)
            beside.fixStations.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            beside.fixStations.setData(idx, 478000.0, FixStationModel.EastingRole)
            beside.fixStations.setData(idx, 4430000.0, FixStationModel.NorthingRole)
            beside.fixStations.setData(idx, 1655.0, FixStationModel.ElevationRole)
            return beside
        }

        // ── The cave page counts the attached file's fixes ──────────────────

        function test_fixStationsLinkCountsAttachedFixes() {
            const cave = makeSavedCaveAttach("node-warnings-fix-count",
                                             "external-centerlines/survex_blocks.svx")
            const cavePage = gotoCavePage(cave)
            const link = findChild(cavePage, "fixStationsLink")
            verify(link !== null, "fixStationsLink must exist")
            tryCompare(link, "text", "1", 20000, "the link counts the file's own *fix")

            cave.fixStations.addFixStation()
            tryCompare(link, "text", "2", 5000, "and the node's own fixes beside it")
        }

        // ── An attached file with no fix of its own ─────────────────────────

        function test_attachedFileUnfixedEntryOpensFixStations() {
            saveProjectAs("node-warnings-unfixed")
            addFixedBesideCave()

            const source = RootData.urlToLocal(TestHelper.tempDirectoryUrl()) + "/unfixed_blocks.svx"
            verify(TestHelper.writeTextFile(source,
                                            "*begin doghill\n"
                                            + "*data normal from to tape compass clino\n"
                                            + "d1 d2 10.0 0 0\n"
                                            + "d2 d3 5.0 90 0\n"
                                            + "*end doghill\n"),
                   "the unfixed copy is written")
            const cave = makeAttachedCave("Unfixed", source)

            const cavePage = gotoCavePage(cave)
            const bannerItem = bannerOn(cavePage)
            tryVerify(() => bannerItem.visible
                      && entryContaining(bannerItem, "has no fixed station") !== null,
                      20000, "the unfixed-file warning reaches the banner")
            const entry = entryContaining(bannerItem, "has no fixed station")
            compare(messageLabel(entry).text,
                    "unfixed_blocks.svx has no fixed station, so it sits at the origin — "
                    + "fix one of its stations.")
            verify(entryContaining(bannerItem, "not tied") === null,
                   "the file sits at the origin, so nothing is untied")

            mouseClick(messageLabel(entry))
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "the line opens the cave's Fix Stations page")
            compare(RootData.pageView.currentPageItem.cave, cave, "the page is the attached cave's")
            const tableView = findChild(RootData.pageView.currentPageItem, "fixStationTableView")
            verify(tableView !== null, "fixStationTableView must exist")
            compare(tableView.count, 0, "the file lists no fix of its own")
            compare(tableView.currentIndex, -1, "the page opens with no row selected")
        }

        // ── A bare *fix in an attached file of a georeferenced project ──────

        function test_attachedFixWithoutCSEntryOpensFixStations() {
            saveProjectAs("node-warnings-bare-fix")

            addFixedBesideCave()

            const cave = makeAttachedCave("Blocks", TestHelper.testcasesDatasetPath(
                                              "external-centerlines/survex_blocks.svx"))
            // A row with no station yet: it places nothing, and puts the
            // attached row at index 1, after the node's own.
            cave.fixStations.addFixStation()

            const cavePage = gotoCavePage(cave)
            const bannerItem = bannerOn(cavePage)
            tryVerify(() => bannerItem.visible
                      && entryContaining(bannerItem, "without a coordinate system") !== null,
                      20000, "the bare-fix warning reaches the banner")
            const entry = entryContaining(bannerItem, "without a coordinate system")
            const text = messageLabel(entry).text
            verify(text.indexOf("survex_blocks.svx") >= 0, "the line names the file: " + text)
            verify(text.indexOf("fixes doghill.d1 without") >= 0, "the line names the station: " + text)
            verify(text.indexOf("add one to the file or remove that fix") >= 0,
                   "the line names the remedy: " + text)

            mouseClick(messageLabel(entry))
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "the line opens the cave's Fix Stations page")
            compare(RootData.pageView.currentPageItem.cave, cave, "the page is the attached cave's")
            const tableView = findChild(RootData.pageView.currentPageItem, "fixStationTableView")
            verify(tableView !== null, "fixStationTableView must exist")
            tryCompare(tableView, "count", 2)
            tryCompare(tableView, "currentIndex", 1, 5000, "the page opens on the attached row")
            tryVerify(() => findChild(tableView, "stationCell.1") !== null
                      && findChild(tableView, "stationCell.1").text === "doghill.d1",
                      5000, "and that row is the file's fix")
        }
    }
}
