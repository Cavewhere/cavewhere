import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    TestCase {
        name: "DataMainPageGlobalCS"
        when: windowShown

        function init() {
            if (GlobalShadowTextInput.coreClickInput !== null) {
                GlobalShadowTextInput.coreClickInput.closeEditor()
            }
            GlobalShadowTextInput.enabled = false

            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000)
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        function gotoDataMainPage() {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "should land on dataMainPage")
        }

        function test_setGlobalCSViaPicker() {
            gotoDataMainPage()

            const picker = findChild(RootData.pageView.currentPageItem, "globalCoordinateSystemComboBox")
            verify(picker !== null, "globalCoordinateSystemComboBox must exist on dataMainPage")

            compare(RootData.region.geoReference.globalCoordinateSystem, "")

            picker.committed("EPSG:32612")
            tryCompare(RootData.region.geoReference, "globalCoordinateSystem", "EPSG:32612")

            picker.committed("EPSG:32613")
            tryCompare(RootData.region.geoReference, "globalCoordinateSystem", "EPSG:32613")

            picker.committed("")
            tryCompare(RootData.region.geoReference, "globalCoordinateSystem", "")
        }

        function test_globalCSPickerHidesGeographic() {
            gotoDataMainPage()

            const picker = findChild(RootData.pageView.currentPageItem, "globalCoordinateSystemComboBox")
            verify(picker !== null, "globalCoordinateSystemComboBox must exist on dataMainPage")

            // Survex's cavern can't emit geographic output, so the project's
            // globalCS picker must hide the Lat/Lon mode entirely.
            verify(!picker.allowGeographic,
                   "DataMainPage globalCS picker must not allow geographic CRSs")

            const modeCombo = findChild(picker, "csModePicker")
            compare(modeCombo.model.length, 3,
                    "Local / UTM / Custom (3 modes) when allowGeographic === false")
        }

        function test_recenterMenuItemReachable() {
            gotoDataMainPage()

            const ctxButton = findChild(RootData.pageView.currentPageItem, "regionContextMenu")
            verify(ctxButton !== null, "regionContextMenu button must exist on dataMainPage")
        }

        function test_geospatialGroupBoxWraps() {
            gotoDataMainPage()

            const dataPage = RootData.pageView.currentPageItem
            const groupBox = findChild(dataPage, "geospatialGroupBox")
            verify(groupBox !== null, "geospatialGroupBox must exist on dataMainPage")

            // The CS picker and the layers link both live inside the
            // Geospatial GroupBox so the screen reads as a single section.
            const picker = findChild(groupBox, "globalCoordinateSystemComboBox")
            verify(picker !== null,
                   "globalCoordinateSystemComboBox must be a descendant of geospatialGroupBox")

            const link = findChild(groupBox, "geospatialLayersLink")
            verify(link !== null,
                   "geospatialLayersLink must be a descendant of geospatialGroupBox")
        }
    }
}
