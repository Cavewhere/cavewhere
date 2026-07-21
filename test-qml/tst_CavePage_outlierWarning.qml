import QtQuick
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

        function addUtm13NFix(name, e, n, z) {
            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(cave.fixStations.count - 1)
            cave.fixStations.setData(idx, name, FixStationModel.StationNameRole)
            cave.fixStations.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            cave.fixStations.setData(idx, e, FixStationModel.EastingRole)
            cave.fixStations.setData(idx, n, FixStationModel.NorthingRole)
            cave.fixStations.setData(idx, z, FixStationModel.ElevationRole)
        }

        // A tight cluster of four good fixes near Boulder, UTM Z13N. Detection
        // needs at least four fixes to have a cluster to judge an outlier from.
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

        // ── Typo'd fix → banner names the station and how far off it is ──────

        function test_bannerAppearsForOutlier() {
            addGoodCluster()
            // ~1000 km east of the cluster: exceeds both the precision floor and
            // the cluster-radius multiple, so it is flagged.
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)

            const b = banner()
            tryVerify(() => b.visible, 1000, "banner appears once a fix is an outlier")
            verify(b.text.indexOf("BAD") >= 0,
                   "banner names the offending station: " + b.text)
            verify(b.text.indexOf("far from the rest of the survey") >= 0,
                   "banner explains the problem: " + b.text)
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
    }
}
