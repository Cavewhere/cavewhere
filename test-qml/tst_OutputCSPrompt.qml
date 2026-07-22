import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// The OutputCSPrompt appears wherever a project has fix stations but no output
// coordinate system: the Data page, the Fix Stations page, and the 3D view. It
// carries an inline projected-CS picker pre-filled with the validator's
// suggestion, an adjacent "Use this" button, and (when the source coordinate is
// bad) a grayed-out picker with guidance to fix the coordinate. Exercising it on
// the pure-UI hosts (Data + Fix Stations) proves the shared wiring; the view
// host reuses the same component and validator API.
MainWindowTest {
    id: rootId

    // A free-floating banner at the view's prompt width, used to assert its
    // inline picker keeps the mode + UTM zone + hemisphere controls on one line.
    OutputCSPrompt {
        id: wideBanner
        width: Theme.outputCSPromptWidth
        suggestedCS: "EPSG:32612"
    }

    TestCase {
        name: "OutputCSPrompt"
        when: windowShown

        property Cave cave: null

        function initTestCase() {
            RootData.project.newProject()
            RootData.region.addCave()
            cave = RootData.region.cave(0)
            cave.name = "PromptCave"
        }

        function init() {
            // Each case starts with an empty project CS and no fixes.
            while (cave.fixStations.count > 0) {
                cave.fixStations.removeAt(0)
            }
            RootData.region.geoReference.globalCoordinateSystem = ""
            gotoDataPage()
        }

        function gotoDataPage() {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem
                      && RootData.pageView.currentPageItem.objectName === "dataMainPage")
        }

        function gotoFixStationPage() {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=PromptCave/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem
                      && RootData.pageView.currentPageItem.objectName === "fixStationPage")
        }

        function prompt() {
            return findChild(RootData.pageView.currentPageItem, "outputCSPrompt")
        }

        function addFix(name, cs, e, n, z) {
            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(cave.fixStations.count - 1)
            cave.fixStations.setData(idx, name, FixStationModel.StationNameRole)
            cave.fixStations.setData(idx, cs, FixStationModel.InputCSRole)
            cave.fixStations.setData(idx, e, FixStationModel.EastingRole)
            cave.fixStations.setData(idx, n, FixStationModel.NorthingRole)
            cave.fixStations.setData(idx, z, FixStationModel.ElevationRole)
        }

        // ── Data page ───────────────────────────────────────────────────────

        function test_dataPagePromptHiddenWithoutFixes() {
            const p = prompt()
            verify(p !== null, "output-CS prompt must exist on the Data page")
            verify(!p.visible, "prompt is hidden with no fix stations")
        }

        function test_dataPagePromptHiddenWhenOutputCSSet() {
            RootData.region.geoReference.globalCoordinateSystem = "EPSG:32612"
            addFix("A1", "EPSG:32612", 500000.0, 4194000.0, 2700.0)

            const p = prompt()
            verify(!p.visible, "no prompt when the project already has an output CS")
        }

        function test_dataPagePromptShowsPickerForFixWithoutOutputCS() {
            addFix("A1", "EPSG:32612", 500000.0, 4194000.0, 2700.0)

            const p = prompt()
            tryVerify(() => p.visible, 1000, "prompt appears once a fix has a coordinate")

            const picker = findChild(p, "outputCSPicker")
            verify(picker !== null, "inline CS picker must exist")
            tryVerify(() => picker.enabled, 1000, "picker is enabled for a good coordinate")

            const useButton = findChild(p, "outputCSUseButton")
            verify(useButton !== null, "Use button must exist")
            tryVerify(() => useButton.enabled, 1000, "Use is enabled when a CS is suggested")

            // The picker is pre-filled with the derived suggestion.
            tryVerify(() => RootData.region.fixStationValidator.suggestedOutputCS === "EPSG:32612",
                      1000, "the suggestion is the fix's own projected CS")
        }

        function test_dataPageShowsSuggestionNameAndCode() {
            addFix("A1", "EPSG:32612", 500000.0, 4194000.0, 2700.0)

            const p = prompt()
            tryVerify(() => p.visible, 1000)

            const name = findChild(p, "outputCSSuggestionName")
            const code = findChild(p, "outputCSSuggestionCode")
            verify(name !== null, "suggestion name label must exist")
            verify(code !== null, "suggestion code label must exist")

            tryVerify(() => code.visible && code.text === "EPSG:32612", 1000,
                      "the authority code is shown: " + code.text)
            tryVerify(() => name.visible && name.text.indexOf("UTM") >= 0, 1000,
                      "the resolved name is shown: " + name.text)
        }

        // The button is driven via its clicked() signal rather than a synthetic
        // mouse hit: this still exercises the production wiring (Button.onClicked
        // → OutputCSPrompt.useSuggested → host sets globalCoordinateSystem), and a
        // raw mouseClick is unreliable on a laid-out page (higher-z interceptors).
        function test_dataPageUseButtonSetsOutputCS() {
            addFix("A1", "EPSG:32612", 500000.0, 4194000.0, 2700.0)

            const p = prompt()
            tryVerify(() => p.visible, 1000, "prompt is up before applying")

            const useButton = findChild(p, "outputCSUseButton")
            tryVerify(() => useButton.enabled, 1000, "Use button is available")
            useButton.clicked()

            tryVerify(() => RootData.region.geoReference.globalCoordinateSystem === "EPSG:32612",
                      1000, "Use adopts the picker's coordinate system")
            tryVerify(() => !p.visible, 1000, "prompt clears once the output CS is set")
        }

        // In the slim info column the picker must wrap rather than let the banner
        // clip its trailing hemisphere combo (the reported bug).
        function test_dataPagePickerFitsWithinBanner() {
            addFix("A1", "EPSG:32612", 500000.0, 4194000.0, 2700.0)

            const p = prompt()
            tryVerify(() => p.visible, 1000)

            const hemi = findChild(p, "csUtmHemisphere")
            verify(hemi !== null, "hemisphere combo exists in UTM mode")
            tryVerify(() => hemi.visible && p.width > 0, 1000, "banner and combo laid out")

            // The combo's right edge must stay within the banner (clip boundary).
            tryVerify(() => hemi.mapToItem(p, hemi.width, 0).x <= p.width + 1, 1000,
                      "hemisphere right edge within banner width " + p.width)
        }

        // At the view's prompt width the picker must not wrap: the hemisphere
        // combo sits on the same row as the mode combo (same top y).
        function test_pickerStaysOnOneLineAtPromptWidth() {
            const mode = findChild(wideBanner, "csModePicker")
            const hemi = findChild(wideBanner, "csUtmHemisphere")
            verify(mode !== null, "mode combo must exist")
            verify(hemi !== null, "hemisphere combo must exist")
            tryVerify(() => hemi.visible && wideBanner.width > 0, 1000, "picker laid out in UTM mode")

            const modeY = mode.mapToItem(wideBanner, 0, 0).y
            const hemiY = hemi.mapToItem(wideBanner, 0, 0).y
            verify(Math.abs(hemiY - modeY) < 4,
                   "picker stays on one line (modeY=" + modeY + " hemiY=" + hemiY + ")")
        }

        // The banner carries the same picker, so the Project settings box hides
        // its redundant Coordinate-system GroupBox while the prompt is up.
        function test_dataPageHidesCSGroupBoxWhilePrompted() {
            const group = findChild(RootData.pageView.currentPageItem, "coordinateSystemGroupContainer")
            verify(group !== null, "coordinate-system GroupBox container must exist")
            verify(group.visible, "GroupBox shows when there is no prompt")

            addFix("A1", "EPSG:32612", 500000.0, 4194000.0, 2700.0)

            const p = prompt()
            tryVerify(() => p.visible, 1000, "prompt is up")
            tryVerify(() => !group.visible, 1000,
                      "GroupBox hides while the banner carries the picker")
        }

        // ── A bad source coordinate grays out the suggestion ────────────────
        // Easting 1478000 is outside UTM zone 13's domain — a data-entry error.

        function test_dataPageBadCoordinateGraysOutPicker() {
            addFix("B", "EPSG:32613", 1478000.0, 4430000.0, 1655.0)

            const p = prompt()
            tryVerify(() => p.visible, 1000, "prompt still appears for a bad coordinate")

            const help = findChild(p, "outputCSCoordinateHelp")
            verify(help !== null, "coordinate-fix guidance must exist")
            tryVerify(() => help.visible, 1000, "guidance shows for an invalid coordinate")

            const picker = findChild(p, "outputCSPicker")
            tryVerify(() => !picker.enabled, 1000, "picker is grayed out for a bad coordinate")

            const useButton = findChild(p, "outputCSUseButton")
            verify(!useButton.enabled, "Use is disabled with no trustworthy suggestion")

            // No trustworthy suggestion, so the resolved name/code are hidden.
            verify(!findChild(p, "outputCSSuggestionName").visible, "name hidden for a bad coordinate")
            verify(!findChild(p, "outputCSSuggestionCode").visible, "code hidden for a bad coordinate")
        }

        // ── A geographic fix suggests its UTM zone ──────────────────────────

        function test_geographicFixSuggestsUtmZone() {
            addFix("A1", CoordinateSystem.wgs84(), -110.0, 40.0, 1500.0)

            const p = prompt()
            tryVerify(() => p.visible, 1000, "prompt appears for a lat/long fix")

            tryVerify(() => RootData.region.fixStationValidator.suggestedOutputCS === "EPSG:32612",
                      1000, "a WGS84 fix at -110/40 derives UTM zone 12N")

            const useButton = findChild(p, "outputCSUseButton")
            tryVerify(() => useButton.enabled, 1000, "Use offers the derived zone")
        }

        // ── Fix Stations page (same shared component) ───────────────────────

        function test_fixStationPagePromptHiddenWithoutFixes() {
            gotoFixStationPage()
            const p = prompt()
            verify(p !== null, "output-CS prompt must exist on the Fix Stations page")
            verify(!p.visible, "prompt hidden with no fixes on the Fix Stations page")
        }

        function test_fixStationPagePromptAppearsAndApplies() {
            addFix("A1", "EPSG:32612", 500000.0, 4194000.0, 2700.0)
            gotoFixStationPage()

            const p = prompt()
            tryVerify(() => p.visible, 1000, "prompt appears on the Fix Stations page")

            const useButton = findChild(p, "outputCSUseButton")
            tryVerify(() => useButton.enabled, 1000)
            useButton.clicked()

            tryVerify(() => RootData.region.geoReference.globalCoordinateSystem === "EPSG:32612",
                      1000, "Use adopts the suggested output CS from the Fix Stations page")
            tryVerify(() => !p.visible, 1000, "prompt clears once the output CS is set")
        }
    }
}
