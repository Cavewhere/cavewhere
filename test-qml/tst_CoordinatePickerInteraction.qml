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

        function gotoPopup() {
            let popup = findChild(gotoRenderer(), "coordinatePickerPopup")
            verify(popup !== null, "Coordinate picker popup found")
            return popup
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
                    "41.5660230, -96.3132030, 2206.890m",
                    "Metric project reports the elevation in meters")

            // The decimals follow the unit, from cwUnits::lengthDecimals.
            RootData.region.unitSystem = Units.Imperial
            compare(popup._formatLatLonElevation(41.566023, -96.313203, 1828.8),
                    "41.5660230, -96.3132030, 6000.00ft",
                    "Imperial project reports the elevation in feet")

            // The 7th decimal is ~1 cm, which a registered LiDAR scan resolves —
            // a pick that lands there must survive the formatting.
            RootData.region.unitSystem = Units.Metric
            compare(popup._formatLatLonElevation(41.5660234, -96.3132037, 2206.89),
                    "41.5660234, -96.3132037, 2206.890m",
                    "Centimeter-scale digits are kept")
        }
    }
}
