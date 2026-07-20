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

        function test_regionInfoBoxWraps() {
            gotoDataMainPage()

            const dataPage = RootData.pageView.currentPageItem
            const infoBox = findChild(dataPage, "regionInfoBox")
            verify(infoBox !== null, "regionInfoBox must exist on dataMainPage")

            // Units, the CS picker and the layers link all live inside the
            // region info box so the screen reads as a single section.
            const units = findChild(infoBox, "unitSystemComboBox")
            verify(units !== null,
                   "unitSystemComboBox must be a descendant of regionInfoBox")

            const picker = findChild(infoBox, "globalCoordinateSystemComboBox")
            verify(picker !== null,
                   "globalCoordinateSystemComboBox must be a descendant of regionInfoBox")

            const link = findChild(infoBox, "geospatialLayersLink")
            verify(link !== null,
                   "geospatialLayersLink must be a descendant of regionInfoBox")
        }

        function test_settingsGatedBehindEdit() {
            gotoDataMainPage()

            const dataPage = RootData.pageView.currentPageItem
            const editButton = findChild(dataPage, "regionSettingsEditButton")
            verify(editButton !== null, "regionSettingsEditButton must exist on dataMainPage")

            const units = findChild(dataPage, "unitSystemComboBox")
            verify(units !== null, "unitSystemComboBox must exist on dataMainPage")
            const picker = findChild(dataPage, "globalCoordinateSystemComboBox")
            verify(picker !== null, "globalCoordinateSystemComboBox must exist on dataMainPage")

            // The editors are hidden until the user opts into editing.
            verify(!editButton.editMode, "info box must start read-only")
            verify(!units.visible, "unit editor must be hidden until Edit is clicked")
            verify(!picker.visible, "CS editor must be hidden until Edit is clicked")

            // Click the actual pencil so the TapHandler → editMode gesture is
            // exercised, not just the flag it sets.
            mouseClick(editButton)
            tryVerify(() => editButton.editMode, 2000, "clicking Edit must enter edit mode")
            tryVerify(() => units.visible, 2000, "unit editor must show in edit mode")
            tryVerify(() => picker.visible, 2000, "CS editor must show in edit mode")

            editButton.editMode = false
        }
    }
}
