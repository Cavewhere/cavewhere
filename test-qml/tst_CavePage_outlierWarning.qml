import QtQuick
import QtQuick.Controls as QC
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    TestCase {
        name: "CavePageOutlierWarning"
        when: windowShown

        property Cave cave: null

        function initTestCase() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.region.addCave()
            cave = RootData.region.cave(0)
            cave.name = "OutlierCave"
            RootData.region.geoReference.globalCoordinateSystem = "EPSG:32613"
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=OutlierCave"
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "cavePage")
        }

        function init() {
            // Drain fix stations between cases so each starts with no outlier.
            while (cave.fixStations.count > 0) {
                cave.fixStations.removeAt(0)
            }
            RootData.region.geoReference.globalCoordinateSystem = "EPSG:32613"
        }

        function cavePage() {
            let p = RootData.pageView.currentPageItem
            verify(p !== null, "cave page must exist")
            return p
        }

        function banner() {
            return findChild(cavePage(), "outlierWarningBanner")
        }

        function fixStationsBadge() {
            return findChild(cavePage(), "fixStationsBadge")
        }

        function bannerProxy() {
            return findChild(cavePage(), "outlierWarningBannerProxy")
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

        // ── No outlier → banner hidden ───────────────────────────────────────

        function test_bannerHiddenWithoutOutlier() {
            const b = banner()
            verify(b !== null, "outlier banner must exist in CavePage")
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
            verify(b.text.indexOf("BAD") >= 0,
                   "banner names the offending station: " + b.text)
            verify(b.text.indexOf("outside the valid range") >= 0,
                   "banner explains the problem: " + b.text)
        }

        // ── The banner renders station names, it doesn't interpret them ─────

        // ErrorHelpArea reads RichText by default so help text elsewhere can
        // carry links, but warning messages quote whatever the user typed into
        // the station-name field. A station named A<b>B must reach the screen
        // intact instead of being swallowed as a tag.
        function test_bannerRendersStationNameLiterally() {
            addUtm13NFix("A<b>B", 1478000.0, 4430000.0, 1655.0)

            const b = banner()
            tryVerify(() => b.visible, 1000, "banner appears for the bad fix")
            compare(b.textFormat, QC.Label.PlainText,
                    "warning text must not be read as markup")
            verify(b.text.indexOf("A<b>B") >= 0,
                   "banner quotes the station name verbatim: " + b.text)
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
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)
            tryVerify(() => badge.visible, 1000, "badge appears once a fix errors")

            // Correcting the coordinate clears the badge.
            const idx = cave.fixStations.index(cave.fixStations.count - 1)
            cave.fixStations.setData(idx, 478000.0, FixStationModel.EastingRole)
            tryVerify(() => !badge.visible, 1000, "badge clears once corrected")
        }

        function test_fixStationBadgeHidesWhenSuppressed() {
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)
            const badge = fixStationsBadge()
            tryVerify(() => badge.visible, 1000, "badge up before suppression")

            const errors = cave.errorModel.errors
            tryCompare(errors, "count", 1)
            errors.setData(errors.index(0, 0), true, ErrorListModel.SuppressedRole)
            tryVerify(() => !badge.visible, 1000, "badge hides once suppressed")
        }
    }
}
