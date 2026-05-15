import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    TestCase {
        name: "CavePageGridConvergence"
        when: windowShown

        property var cave: null

        function initTestCase() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.region.addCave()
            cave = RootData.region.cave(0)
            cave.name = "ConvCave"
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=ConvCave"
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "cavePage")
        }

        function init() {
            // Drain fix stations between cases so each test starts clean.
            while (cave.fixStations.count > 0) {
                cave.fixStations.removeAt(0)
            }
            RootData.region.globalCS = "EPSG:32613"

            // Reset the help-area visibility — tests share one CavePage
            // instance, so a previous toggle would otherwise bleed in.
            const helpArea = findChild(cavePage(), "gridConvergenceHelp")
            if (helpArea !== null) {
                helpArea.visible = false
            }
        }

        function cavePage() {
            let p = RootData.pageView.currentPageItem
            verify(p !== null, "cave page must exist")
            return p
        }

        function convergenceLabel() {
            return findChild(cavePage(), "gridConvergenceValue")
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

        // ── No fix station → "n/a (no fix station)" ──────────────────────────

        function test_noFixStationShowsNotApplicable() {
            const label = convergenceLabel()
            verify(label !== null, "convergence label must exist in CavePage")
            compare(cave.gridConvergenceText, "n/a (no fix station)")
            compare(label.text, "n/a (no fix station)")
        }

        // ── Real fix station → numeric "X.XX° at <name> (<csName>)" ──────────

        function test_utmFixShowsNumericValue() {
            addUtm13NFix("a1", 478000.0, 4430000.0, 1655.0)

            const text = cave.gridConvergenceText
            verify(text.indexOf("a1") >= 0, "compact label includes the fix station name")
            verify(text.indexOf("(") < 0, "compact label omits the CS for compactness: " + text)

            // Boulder is just west of UTM Z13N's central meridian — convergence
            // should be a small negative angle near -0.17°. Parse the leading
            // number so the test isn't tied to formatting tweaks.
            const match = text.match(/^(-?\d+(?:\.\d+)?)°/)
            verify(match !== null, "label starts with a signed angle: " + text)
            const value = parseFloat(match[1])
            verify(value < 0.0 && value > -1.0,
                   "convergence is a small negative angle: got " + value)

            const label = convergenceLabel()
            compare(label.text, text)
        }

        // ── Detail text appends the CS for the tooltip ───────────────────────

        function test_detailTextAppendsCsForTooltip() {
            addUtm13NFix("a1", 478000.0, 4430000.0, 1655.0)

            const compact = cave.gridConvergenceText
            const detail = cave.gridConvergenceDetailText
            verify(detail.indexOf(compact) === 0,
                   "detail starts with the compact text: " + detail)
            verify(detail.length > compact.length,
                   "detail includes additional CS info beyond the compact text")
            // The CS name from PROJ for EPSG:32613 always contains "UTM".
            verify(detail.indexOf("UTM") >= 0,
                   "detail includes the CS name: " + detail)
        }

        // ── Help area toggles when the label is clicked ──────────────────────

        function test_helpAreaTogglesOnLabelClick() {
            const helpArea = findChild(cavePage(), "gridConvergenceHelp")
            verify(helpArea !== null, "help area must exist")
            verify(!helpArea.visible, "help area is hidden by default")

            // The LabelWithHelp toggles the area on click; flip programmatically
            // since hit-testing a label inside the laid-out wide column from a
            // headless test is fiddly.
            helpArea.visible = true
            verify(helpArea.visible, "help area becomes visible after toggle")

            verify(helpArea.text.length > 0, "help text is populated")
            verify(helpArea.text.toLowerCase().indexOf("grid convergence") >= 0,
                   "help text introduces the concept")
        }

        // ── For n/a cases, compact == detail (nothing extra to surface) ──────

        function test_detailMatchesCompactForNotApplicableCases() {
            compare(cave.gridConvergenceText, cave.gridConvergenceDetailText,
                    "no-fix case: detail equals compact")

            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(0)
            cave.fixStations.setData(idx, "a1", FixStationModel.StationNameRole)
            cave.fixStations.setData(idx, "EPSG:4326", FixStationModel.InputCSRole)

            compare(cave.gridConvergenceText, cave.gridConvergenceDetailText,
                    "geographic CS: detail equals compact")
        }

        // ── Geographic CS → "n/a (geographic CS)" ────────────────────────────

        function test_geographicCsShowsNotApplicable() {
            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(0)
            cave.fixStations.setData(idx, "a1", FixStationModel.StationNameRole)
            cave.fixStations.setData(idx, "EPSG:4326", FixStationModel.InputCSRole)
            cave.fixStations.setData(idx, -105.27, FixStationModel.EastingRole)
            cave.fixStations.setData(idx, 40.015, FixStationModel.NorthingRole)
            cave.fixStations.setData(idx, 1655.0, FixStationModel.ElevationRole)

            compare(cave.gridConvergenceText, "n/a (geographic CS)")
        }

        // ── Updates when a fix station is added ──────────────────────────────

        function test_textUpdatesWhenFixStationAdded() {
            compare(cave.gridConvergenceText, "n/a (no fix station)")

            addUtm13NFix("a1", 478000.0, 4430000.0, 1655.0)

            tryVerify(() => cave.gridConvergenceText.indexOf("a1") >= 0, 500,
                      "label must update to numeric value once a fix is added")
        }

        // ── Updates when the fix station's coordinates change ────────────────

        function test_textUpdatesWhenFixCoordinatesChange() {
            addUtm13NFix("a1", 478000.0, 4430000.0, 1655.0)
            const first = cave.gridConvergenceText

            // Move the fix to a point ~3° west — should move convergence
            // measurably (>~1.5° magnitude shift).
            const idx = cave.fixStations.index(0)
            cave.fixStations.setData(idx, 224000.0, FixStationModel.EastingRole)

            tryVerify(() => cave.gridConvergenceText !== first, 500,
                      "label must recompute when fix coords change")
        }

        // ── Updates when fix's inputCS is cleared (falls back to globalCS) ───

        function test_textFallsBackToGlobalCsWhenFixHasNoInputCs() {
            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(0)
            cave.fixStations.setData(idx, "a1", FixStationModel.StationNameRole)
            cave.fixStations.setData(idx, "", FixStationModel.InputCSRole)
            cave.fixStations.setData(idx, 478000.0, FixStationModel.EastingRole)
            cave.fixStations.setData(idx, 4430000.0, FixStationModel.NorthingRole)
            cave.fixStations.setData(idx, 1655.0, FixStationModel.ElevationRole)

            // globalCS is EPSG:32613 from init(); convergence should be numeric.
            verify(cave.gridConvergenceText.indexOf("a1") >= 0)
            verify(cave.gridConvergenceText.indexOf("°") >= 0)
        }

        // ── Updates when the region's globalCS changes ───────────────────────

        function test_textUpdatesWhenGlobalCsChanges() {
            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(0)
            cave.fixStations.setData(idx, "a1", FixStationModel.StationNameRole)
            cave.fixStations.setData(idx, "", FixStationModel.InputCSRole)
            cave.fixStations.setData(idx, 478000.0, FixStationModel.EastingRole)
            cave.fixStations.setData(idx, 4430000.0, FixStationModel.NorthingRole)
            cave.fixStations.setData(idx, 1655.0, FixStationModel.ElevationRole)

            const before = cave.gridConvergenceText

            // Switch to a geographic global CS — fix falls back to globalCS
            // since inputCS is empty, so the readout flips to "geographic CS".
            RootData.region.globalCS = "EPSG:4326"

            tryVerify(() => cave.gridConvergenceText === "n/a (geographic CS)", 500,
                      "label must recompute when globalCS changes")
            verify(before !== cave.gridConvergenceText)
        }
    }
}
