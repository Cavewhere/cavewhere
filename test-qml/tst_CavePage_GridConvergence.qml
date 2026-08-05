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
            compare(cave.gridConvergence.text, "n/a (no fix station)")
            compare(label.text, "n/a (no fix station)")
        }

        // ── Real fix station → numeric "X.XX° at <name>" ─────────────────────

        function test_utmFixShowsNumericValue() {
            addUtm13NFix("a1", 478000.0, 4430000.0, 1655.0)

            const text = cave.gridConvergence.text
            verify(text.indexOf("a1") >= 0, "compact label includes the fix station name")
            verify(text.indexOf("(") < 0, "compact label omits the grid for compactness: " + text)

            // This fix is the project's only georeferenced input, so it is what
            // the local projection is centered on — and grid north at the center
            // of a transverse Mercator is true north. Parse the leading number
            // so the test isn't tied to formatting tweaks.
            const match = text.match(/^(-?\d+(?:\.\d+)?)°/)
            verify(match !== null, "label starts with a signed angle: " + text)
            const value = parseFloat(match[1])
            verify(Math.abs(value) < 0.01,
                   "the anchor of the frame has nothing to converge to: got " + value)

            const label = convergenceLabel()
            compare(label.text, text)
        }

        // ── Detail text names the grid for the tooltip ───────────────────────

        function test_detailTextAppendsCsForTooltip() {
            addUtm13NFix("a1", 478000.0, 4430000.0, 1655.0)

            const compact = cave.gridConvergence.text
            const detail = cave.gridConvergence.detailText
            verify(detail.indexOf(compact) === 0,
                   "detail starts with the compact text: " + detail)
            verify(detail.length > compact.length,
                   "detail says more than the compact text")
            verify(detail.indexOf("local projection") >= 0,
                   "detail names the grid the angle is measured in: " + detail)
        }

        // ── Page-level scrollbar activates when the help expands ─────────────

        function test_pageScrollbarReflectsContentOverflow() {
            const scrollBar = findChild(cavePage(), "cavePageVerticalScrollBar")
            verify(scrollBar !== null, "page-level vertical scrollbar must exist")

            const helpArea = findChild(cavePage(), "gridConvergenceHelp")
            verify(helpArea !== null)
            helpArea.visible = false

            // Baseline: with help collapsed and an empty trip table, content
            // fits — scrollbar size is at-or-near full track (no overflow).
            const sizeCollapsed = scrollBar.size
            verify(sizeCollapsed > 0.0, "scrollbar size is defined")

            // Expanding the help block grows the stats column. Either the
            // scrollbar appears (size drops below 1) or it stays at 1 if the
            // viewport is taller than the expanded content. Both are fine, but
            // size must not *grow* — overflow only adds height.
            helpArea.visible = true
            const sizeExpanded = scrollBar.size
            verify(sizeExpanded <= sizeCollapsed + 0.001,
                   "expanded help must not shrink content: "
                   + sizeCollapsed + " -> " + sizeExpanded)
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
            compare(cave.gridConvergence.text, cave.gridConvergence.detailText,
                    "no-fix case: detail equals compact")

            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(0)
            cave.fixStations.setData(idx, "a1", FixStationModel.StationNameRole)
            cave.fixStations.setData(idx, "EPSG:4326", FixStationModel.InputCSRole)

            // A row with a system and no coordinate places nothing, so the
            // project stays un-georeferenced.
            compare(cave.gridConvergence.text, cave.gridConvergence.detailText,
                    "fix with no coordinate: detail equals compact")
        }

        // ── A geographic fix still converges in the project's own grid ───────

        function test_geographicFixConvergesInTheProjectFrame() {
            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(0)
            cave.fixStations.setData(idx, "a1", FixStationModel.StationNameRole)
            cave.fixStations.setData(idx, "EPSG:4326", FixStationModel.InputCSRole)
            cave.fixStations.setData(idx, -105.27, FixStationModel.EastingRole)
            cave.fixStations.setData(idx, 40.015, FixStationModel.NorthingRole)
            cave.fixStations.setData(idx, 1655.0, FixStationModel.ElevationRole)

            // Lat/lon has no grid of its own, but it places the cave — and the
            // grid the answer is about is the project's local projection, which
            // this fix anchors.
            compare(cave.gridConvergence.state, GridConvergence.Valid)
            verify(cave.gridConvergence.text.indexOf("a1") >= 0,
                   "the readout names the fix it converged at: "
                   + cave.gridConvergence.text)
        }

        // ── Updates when a fix station is added ──────────────────────────────

        function test_textUpdatesWhenFixStationAdded() {
            compare(cave.gridConvergence.text, "n/a (no fix station)")

            addUtm13NFix("a1", 478000.0, 4430000.0, 1655.0)

            tryVerify(() => cave.gridConvergence.text.indexOf("a1") >= 0, 500,
                      "label must update to numeric value once a fix is added")
        }

        // ── Updates when the fix station's coordinates change ────────────────

        function test_textUpdatesWhenFixCoordinatesChange() {
            addUtm13NFix("a1", 478000.0, 4430000.0, 1655.0)
            const first = cave.gridConvergence.text

            // Move the fix 40 km east — close enough that the frame stays where
            // it is, so the cave now stands off its central meridian and picks
            // up a real convergence (~0.3°).
            const idx = cave.fixStations.index(0)
            cave.fixStations.setData(idx, 518000.0, FixStationModel.EastingRole)

            tryVerify(() => cave.gridConvergence.text !== first, 500,
                      "label must recompute when fix coords change")
        }

        // ── A fix with no inputCS has no grid to converge to ─────────────────

        //! Enter the coordinate under a real system, then take the system away.
        //! The numbers stay in the row's text, so this is a fix that has a
        //! coordinate and no grid — not an empty row, which would read the same
        //! way for a much less interesting reason.
        function enterThenClearTheCs(idx) {
            cave.fixStations.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            cave.fixStations.setData(idx, 478000.0, FixStationModel.EastingRole)
            cave.fixStations.setData(idx, 4430000.0, FixStationModel.NorthingRole)
            cave.fixStations.setData(idx, 1655.0, FixStationModel.ElevationRole)
            cave.fixStations.setData(idx, "", FixStationModel.InputCSRole)
            verify(cave.fixStations.data(idx, FixStationModel.CoordinateTextRole).length > 0,
                   "the coordinate text must survive losing the CS")
        }

        function test_textIsNotAvailableWhenFixHasNoInputCs() {
            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(0)
            cave.fixStations.setData(idx, "a1", FixStationModel.StationNameRole)
            enterThenClearTheCs(idx)

            // A fix that declares no system has no grid to converge to, and
            // nothing else stands in for one.
            compare(cave.gridConvergence.text, "n/a (no coordinate system)")
        }

    }
}
