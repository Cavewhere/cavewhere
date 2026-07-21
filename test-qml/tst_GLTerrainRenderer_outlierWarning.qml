import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    TestCase {
        name: "GLTerrainRendererOutlierWarning"
        when: windowShown

        property Cave cave: null

        function initTestCase() {
            RootData.project.newProject()
            RootData.region.addCave()
            cave = RootData.region.cave(0)
            cave.name = "OutlierCave"
            RootData.region.geoReference.globalCoordinateSystem = "EPSG:32613"
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "viewPage")
        }

        function init() {
            // Drain fix stations between cases so each starts with no outlier.
            while (cave.fixStations.count > 0) {
                cave.fixStations.removeAt(0)
            }
            RootData.region.geoReference.globalCoordinateSystem = "EPSG:32613"

            // A routing case can navigate away from the view; return to it so the
            // next case (alphabetical order) finds the overlay again.
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "viewPage")
        }

        function viewPage() {
            let p = RootData.pageView.currentPageItem
            verify(p !== null, "view page must exist")
            return p
        }

        function overlay() {
            return findChild(viewPage(), "fixStationOutlierBox")
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

        // ── No outlier → overlay hidden ─────────────────────────────────────

        function test_overlayHiddenWithoutOutlier() {
            const o = overlay()
            verify(o !== null, "outlier overlay must exist in the render view")
            verify(!o.visible, "overlay is hidden with no fix stations")

            addGoodCluster()
            verify(!o.visible, "a clean cluster raises no overlay")
        }

        // ── Typo'd fix → overlay names the cave and routes to its fix stations ─

        function test_overlayAppearsForOutlier() {
            addGoodCluster()
            // ~1000 km east of the cluster: exceeds both the precision floor and
            // the cluster-radius multiple, so it is flagged.
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)

            const o = overlay()
            tryVerify(() => o.visible, 1000, "overlay appears once a fix is an outlier")
            verify(o.text.indexOf("OutlierCave") >= 0,
                   "overlay names the offending cave: " + o.text)
            verify(o.text.indexOf("Fix Stations") >= 0,
                   "overlay hints toward the cave's fix stations: " + o.text)
        }

        // ── Activating the link routes to the offending cave's fix stations ──

        function test_overlayLinkRoutesToFixStations() {
            addGoodCluster()
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)

            const o = overlay()
            tryVerify(() => o.visible, 1000, "overlay is up before the link is used")

            // Emitting the HelpBox linkActivated signal drives the overlay's
            // navigation handler, exactly as a click on the rich-text link would.
            o.linkActivated("fixStations")

            tryVerify(() => RootData.pageSelectionModel.currentPageAddress.indexOf("Cave=OutlierCave") >= 0,
                      1000,
                      "the link routes to the offending cave: "
                      + RootData.pageSelectionModel.currentPageAddress)
        }

        // ── Correcting the coordinate hides the overlay ─────────────────────

        function test_overlayClearsWhenCoordinateCorrected() {
            addGoodCluster()
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)

            const o = overlay()
            tryVerify(() => o.visible, 1000, "overlay is up before the correction")

            const idx = cave.fixStations.index(cave.fixStations.count - 1)
            cave.fixStations.setData(idx, 478000.0, FixStationModel.EastingRole)

            tryVerify(() => !o.visible, 1000,
                      "overlay clears once the coordinate is corrected")
        }
    }
}
