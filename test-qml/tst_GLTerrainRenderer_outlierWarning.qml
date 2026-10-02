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
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "viewPage")
        }

        function init() {
            // Drain fix stations between cases so each starts with no outlier.
            while (cave.fixStations.count > 0) {
                cave.fixStations.removeAt(0)
            }

            // The overlay quotes the cave name, so a case that renames the cave
            // would otherwise leak that name into the ones after it.
            cave.name = "OutlierCave"

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

        // The label HelpBox puts the rich text in. It carries no objectName, so
        // it is found by the one thing that distinguishes it: only a text item
        // answers linkAt().
        function overlayLabel() {
            const kids = overlay().children
            for (let i = 0; i < kids.length; i++) {
                if (typeof kids[i].linkAt === "function") {
                    return kids[i]
                }
            }
            return null
        }

        // Where the "Fix Stations" anchor actually landed after layout, asked of
        // the label itself rather than guessed from the wording.
        function linkPoint(label) {
            const kStep = 2
            for (let y = 0; y < label.height; y += kStep) {
                for (let x = 0; x < label.width; x += kStep) {
                    if (label.linkAt(x, y) === "fixStations") {
                        return Qt.point(x, y)
                    }
                }
            }
            return null
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

        // Four good fixes near Boulder, UTM Z13N. The first anchors the
        // project's frame, so all four sit within meters of its origin.
        function addGoodFixes() {
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

            addGoodFixes()
            verify(!o.visible, "clean fixes raise no overlay")
        }

        // ── Typo'd fix → overlay names the cave and routes to its fix stations ─

        function test_overlayAppearsForOutlier() {
            addGoodFixes()
            // ~1000 km east of the others, and so of the frame origin they
            // anchored: past the threshold, so it is flagged.
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)

            const o = overlay()
            tryVerify(() => o.visible, 1000, "overlay appears once a fix is an outlier")
            verify(o.text.indexOf("OutlierCave") >= 0,
                   "overlay names the offending cave: " + o.text)
            verify(o.text.indexOf("Fix Stations") >= 0,
                   "overlay hints toward the cave's fix stations: " + o.text)
        }

        // ── The cave name is escaped into the overlay's markup ───────────────

        // This overlay really is rich text — the "Fix Stations" link is what
        // makes it actionable — so the summary glued in front of that link
        // carries a name the user chose. Angle brackets can't get this far:
        // cwNameUtils::sanitizeFileName rejects them, since a cave name becomes
        // a directory name. An ampersand is accepted, and starts an entity.
        function test_overlayEscapesCaveNameIntoMarkup() {
            cave.name = "Bat & Ball"
            compare(cave.name, "Bat & Ball", "the ampersand must survive the name validator")

            addGoodFixes()
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)

            const o = overlay()
            tryVerify(() => o.visible, 1000, "overlay appears once a fix is an outlier")
            verify(o.text.indexOf("Bat &amp; Ball") >= 0,
                   "the cave name reaches the label escaped: " + o.text)
            verify(o.text.indexOf("<a href=\"fixStations\">") >= 0,
                   "the overlay's own link survives the escaping: " + o.text)
        }

        // ── Activating the link routes to the offending cave's fix stations ──

        function test_overlayLinkRoutesToFixStations() {
            addGoodFixes()
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)

            const o = overlay()
            tryVerify(() => o.visible, 1000, "overlay is up before the link is used")

            // Drive the navigation handler on its own, so a routing failure is
            // told apart from a click that never lands — which is what
            // test_overlayLinkAcceptsARealClick covers.
            o.linkActivated("fixStations")

            // The whole address, not just the cave part: the banner offers to
            // open the Fix Stations page, and landing on the cave page instead
            // leaves the user to find it (#627).
            tryVerify(() => RootData.pageSelectionModel.currentPageAddress
                              === "Source/Data/Cave=OutlierCave/Fix Stations",
                      1000,
                      "the link routes to the offending cave's fix stations: "
                      + RootData.pageSelectionModel.currentPageAddress)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      1000, "and the Fix Stations page is what is shown")
        }

        // ── The link is reachable by an actual click ────────────────────────

        // Emitting linkActivated proves the handler routes; it says nothing
        // about whether a press ever reaches the label. The banner sits inside
        // the render view underneath LeadView and LinePlotLabelView, which fill
        // the same area and carry tap-away handlers, so the click is the only
        // thing that tests the z-order the banner depends on.
        function test_overlayLinkAcceptsARealClick() {
            addGoodFixes()
            addUtm13NFix("BAD", 1478000.0, 4430000.0, 1655.0)

            const o = overlay()
            tryVerify(() => o.visible, 1000, "overlay is up before the link is clicked")

            const label = overlayLabel()
            verify(label !== null, "the overlay's text label must be findable")

            const p = linkPoint(label)
            verify(p !== null, "the \"Fix Stations\" link must occupy a hit-testable point")

            mouseClick(label, p.x, p.y)

            tryVerify(() => RootData.pageSelectionModel.currentPageAddress
                              === "Source/Data/Cave=OutlierCave/Fix Stations",
                      1000,
                      "clicking the link routes to the fix stations: "
                      + RootData.pageSelectionModel.currentPageAddress)
        }

        // ── Correcting the coordinate hides the overlay ─────────────────────

        function test_overlayClearsWhenCoordinateCorrected() {
            addGoodFixes()
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
