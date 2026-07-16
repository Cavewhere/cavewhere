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

        // Verifies the interaction lifecycle: the floating toggle button activates
        // the picker via InteractionManager, which disables the turn-table; toggling
        // off restores the turn-table and clears the current pick.
        //
        // The ray-cast itself (cwCamera::frustrumRay → cwGeometryItersecter →
        // cwGeoPoint → cwCoordinateTransform) is covered by the C++ unit tests
        // for those classes plus the manual smoke test in the plan file.
        function test_lifecycle() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
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

            let turnTable = renderer.turnTableInteraction
            let picker = renderer.coordinatePickerInteraction
            verify(picker !== null, "Coordinate picker exposed on renderer")
            verify(turnTable !== null, "Turn table exposed on renderer")

            let pickButton = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer->coordinatePickerButton")
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

            let popup = findChild(renderer, "coordinatePickerPopup")
            verify(popup !== null, "Coordinate picker popup found")

            compare(RootData.region.geoReference.globalCoordinateSystem, "",
                    "Fresh project has no coordinate system")
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
    }
}
