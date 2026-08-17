import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "CoordinatePickerInteraction"
        when: windowShown

        // Every test here works through the 3D renderer on the View page: show
        // the page, then hand back the renderer once the page has built it.
        function gotoRenderer() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "View page should be active")

            let renderer = null
            tryVerify(() => {
                renderer = ObjectFinder.findObjectByChain(rootId.mainWindow,
                    "rootId->viewPage->SplitView->renderer")
                return renderer !== null
            }, 5000, "Renderer found")
            return renderer
        }

        //! Hand the view back the way each test found it — turn-table active,
        //! no pick standing — whichever tool a test armed and however it ended.
        function cleanup() {
            const view = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer")
            if (view) {
                view.coordinatePickerInteraction.deactivate()
                view.coordinatePickerInteraction.clearPick()
            }
        }

        function gotoPopup() {
            let popup = findChild(gotoRenderer(), "coordinatePickerPopup")
            verify(popup !== null, "Coordinate picker popup found")
            return popup
        }

        //! The frame an anchoring fix derives here, in the conterminous US, is
        //! plate-fixed to NAD83(2011) — so a project anchored with it has a
        //! context datum that is not the WGS84 fallback.
        readonly property double anchorLatitude: 37.0
        readonly property double anchorLongitude: -84.0
        readonly property string plateFixedDatum: "EPSG:6318"

        //! Moving the anchor here re-derives the frame on another plate, so the
        //! region's datum becomes ETRS89 without the pick being touched.
        readonly property double europeanLatitude: 48.0
        readonly property double europeanLongitude: 10.0
        readonly property string europeanDatum: "EPSG:4258"

        //! Anchor the current project with one fix station in the US, giving the
        //! region both a frame and a context datum of its own. Hands back the fix
        //! station model, whose row 0 is the anchor.
        function anchorProjectInTheUs() {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = "DatumCave"

            const model = cave.fixStations
            // The system goes in before the numbers — it decides which axis the
            // coordinate leads with.
            compare(model.addFixStation("A1"), 0)
            model.setData(model.index(0), "EPSG:4326", FixStationModel.InputCSRole)
            model.setData(model.index(0), anchorLongitude, FixStationModel.EastingRole)
            model.setData(model.index(0), anchorLatitude, FixStationModel.NorthingRole)
            model.setData(model.index(0), 300.0, FixStationModel.ElevationRole)

            tryVerify(() => RootData.region.geoReference.hasCoordinateSystem, 5000,
                      "the anchoring fix should have given the project a frame")
            tryCompare(RootData.region, "defaultFixDatum", plateFixedDatum, 5000)
            return model
        }

        //! Steps the terrain probe walks the view in.
        readonly property int probeStep: 20

        //! Picks a point in \a view the ray-cast lands on, and leaves that pick
        //! standing. Probed rather than assumed: what the camera frames depends
        //! on the project and on the size of the window.
        function pickTerrain(view) {
            const picker = view.coordinatePickerInteraction
            RootData.futureManagerModel.waitForFinished()
            waitForRendering(rootId)

            let found = null
            for (let x = probeStep; x < view.width && found === null; x += probeStep) {
                for (let y = probeStep; y < view.height; y += probeStep) {
                    picker.pick(Qt.point(x, y))
                    if (picker.hasPick) {
                        found = Qt.point(x, y)
                        break
                    }
                }
            }
            verify(found !== null, "the loaded project should put geometry under the camera")
            return found
        }

        // Verifies the interaction lifecycle: the sidebar tool-rail button activates
        // the picker via InteractionManager, which disables the turn-table; toggling
        // off restores the turn-table and clears the current pick.
        //
        // The ray-cast itself (cwCamera::frustumRay → cwGeometryItersecter →
        // cwGeoPoint → cwCoordinateTransform) is covered by the C++ unit tests
        // for those classes plus the manual smoke test in the plan file.
        function test_lifecycle() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            let renderer = gotoRenderer()

            let turnTable = renderer.turnTableInteraction
            let picker = renderer.coordinatePickerInteraction
            verify(picker !== null, "Coordinate picker exposed on renderer")
            verify(turnTable !== null, "Turn table exposed on renderer")

            // The tool now lives in the main sidebar's tool rail, not the view.
            let pickButton = findChild(rootId.mainWindow, "coordinatePickerButton")
            verify(pickButton !== null, "Pick button found")

            // Default: turn-table is the active interaction.
            verify(turnTable.enabled === true, "Turn-table active by default")
            verify(picker.enabled === false, "Picker inactive by default")
            verify(picker.hasPick === false, "No pick on startup")

            // Toggle the picker on — the button calls picker.activate(), which
            // routes through InteractionManager and disables the turn table.
            mouseClick(pickButton)
            tryVerify(() => picker.enabled === true, 1000, "Picker enabled after toggle on")
            tryVerify(() => turnTable.enabled === false, 1000, "Turn-table disabled while picker active")

            // pick() on an empty-area click leaves hasPick false — no crash, no state change.
            picker.pick(Qt.point(5, 5))
            tryCompare(picker, "hasPick", false, 500)

            // Toggle off — should restore the turn-table as default interaction.
            mouseClick(pickButton)
            tryVerify(() => turnTable.enabled === true, 1000, "Turn-table restored when picker toggled off")
            tryVerify(() => picker.enabled === false, 1000, "Picker disabled after toggle off")
            tryVerify(() => picker.hasPick === false, 1000, "Pick cleared when picker deactivated")
        }

        // Without a coordinate system the pick can't be georeferenced, so the
        // popup must guide the user to set one instead of showing a blank body
        // (issue #571). The link navigates to the Data page, where the region's
        // coordinate system is set.
        function test_needsCoordinateSystemGuidance() {
            RootData.newProject()

            let popup = gotoPopup()

            verify(!RootData.region.geoReference.hasCoordinateSystem,
                   "geoReference reports no coordinate system")
            verify(!popup.picker.hasCoordinateSystem,
                   "Picker mirrors the geo-reference's has-CS state")
            verify(popup._needsCoordinateSystem,
                   "Popup should offer coordinate-system guidance when there's no CS")

            popup._gotoCoordinateSystem()
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "Link should navigate to the Data page")
        }

        // The pick reads as one pasteable "lat, lon, elevation" triple. Only the
        // elevation carries a unit, and it follows the project's unit system —
        // a metric project reads meters, an imperial one feet.
        function test_latLonElevationFormat() {
            RootData.newProject()

            let popup = gotoPopup()

            RootData.region.unitSystem = Units.Metric
            compare(popup._formatLatLonElevation(41.566023, -96.313203, 2206.89),
                    "41.56602300, -96.31320300, 2206.890m",
                    "Metric project reports the elevation in meters")

            // The decimals follow the unit, from cwUnits::lengthDecimals.
            RootData.region.unitSystem = Units.Imperial
            compare(popup._formatLatLonElevation(41.566023, -96.313203, 1828.8),
                    "41.56602300, -96.31320300, 6000.00ft",
                    "Imperial project reports the elevation in feet")

            // The 8th decimal (~1 mm of latitude) has to survive formatting,
            // where the 7th used to round it away to the centimeter.
            RootData.region.unitSystem = Units.Metric
            compare(popup._formatLatLonElevation(41.56602341, -96.31320372, 2206.89),
                    "41.56602341, -96.31320372, 2206.890m",
                    "Millimeter-scale digits are kept")
        }

        // The readout reports on the datum a fix typed into this project
        // defaults to, and says which one that is: a project anchored in the US
        // reads NAD83(2011), a project with nothing to anchor it reads the WGS84
        // fallback.
        function test_theHeaderNamesTheContextDatum() {
            RootData.newProject()

            let popup = gotoPopup()
            let header = findChild(popup, "LatLonHeader")
            verify(header !== null, "the lat/lon section should carry a header")

            compare(popup.picker.datum, CoordinateSystem.wgs84(),
                    "a project with no context falls back to WGS84")
            compare(header.text, "WGS84 (lat, lon, elevation)")

            anchorProjectInTheUs()

            tryCompare(popup.picker, "datum", plateFixedDatum, 5000,
                       "the picker should follow the region's context datum")
            tryCompare(header, "text", "NAD83(2011) (lat, lon, elevation)", 5000,
                       "the header should name the datum the numbers are on")
        }

        // The datum belongs in the header, not in the clipboard: the copied
        // string is the three numbers a coordinate field reads, and nothing else.
        function test_theCopiedTextCarriesNoDatumName() {
            RootData.newProject()
            anchorProjectInTheUs()

            let popup = gotoPopup()
            let field = findChild(popup, "LatLonField")
            verify(field !== null, "the lat/lon section should carry a value field")

            tryCompare(popup, "_datumName", "NAD83(2011)", 5000)
            verify(/^-?[0-9.]+, -?[0-9.]+, -?[0-9.]+(m|ft)$/.test(field.text),
                   "the copied value should be numbers and an elevation unit: " + field.text)
            compare(field.text.indexOf(popup._datumName), -1,
                    "the copied value should stay free of the datum name")
        }

        // A real click on the terrain, read out end to end: the transform to the
        // context datum builds, the numbers land near the anchor, and the
        // section the user copies from is the one on screen. Then the anchor
        // moves onto another plate — the frame and the datum both re-derive, and
        // the pick already showing follows them rather than keeping the numbers
        // it was picked with.
        function test_aPickReadsOutOnTheContextDatumAndFollowsIt() {
            RootData.newProject()
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"))
            const model = anchorProjectInTheUs()

            let popup = gotoPopup()
            let view = gotoRenderer()
            let picker = view.coordinatePickerInteraction
            tryCompare(picker, "datum", plateFixedDatum, 5000)

            // The readout only comes up for the picker's own tool, so arm it the
            // way the user does before asking what the popup shows.
            let pickButton = findChild(rootId.mainWindow, "coordinatePickerButton")
            verify(pickButton !== null, "the tool rail should carry the pick button")
            mouseClick(pickButton)
            tryCompare(picker, "enabled", true, 5000)

            pickTerrain(view)
            tryCompare(popup, "visible", true, 5000)

            verify(picker.hasLatLon,
                   "the pick should transform onto the project's own datum")
            fuzzyCompare(picker.latitude, anchorLatitude, 1.0)
            fuzzyCompare(picker.longitude, anchorLongitude, 1.0)

            let field = findChild(popup, "LatLonField")
            verify(field !== null, "the lat/lon section should carry a value field")
            compare(field.text.indexOf(picker.latitude.toFixed(popup._degreePrecision)), 0,
                    "the copied value should lead with the latitude picked: " + field.text)

            // One reading is on screen: the numbers, not the elevation-only
            // fallback and not the guidance for an unpositioned project.
            let latLonSection = findChild(popup, "LatLonSection")
            let elevationSection = findChild(popup, "ElevSection")
            let guidance = findChild(popup, "needsCoordinateSystem")
            compare(latLonSection.visible, true)
            compare(elevationSection.visible, false,
                    "a pick that reads out in lat/lon keeps the fallback down")
            compare(guidance.visible, false)

            let header = findChild(popup, "LatLonHeader")
            const pickedLongitude = picker.longitude

            model.setData(model.index(0), europeanLongitude, FixStationModel.EastingRole)
            model.setData(model.index(0), europeanLatitude, FixStationModel.NorthingRole)

            tryCompare(picker, "datum", europeanDatum, 5000,
                       "the picker should follow the region onto the new plate")
            verify(picker.hasPick, "the pick stands through the frame change")
            tryVerify(() => Math.abs(picker.longitude - pickedLongitude) > 1.0, 5000,
                      "the numbers under the marker should be the ones the new frame says")
            fuzzyCompare(picker.longitude, europeanLongitude, 1.0)
            fuzzyCompare(picker.latitude, europeanLatitude, 1.0)
            compare(header.text, "ETRS89 (lat, lon, elevation)")

            mouseClick(pickButton)
        }
    }
}
