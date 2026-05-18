import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    TestCase {
        name: "FixStationsRoundTrip"
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

        function setGlobalCSViaPicker(cs) {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000)
            const dataPage = RootData.pageView.currentPageItem
            const picker = findChild(dataPage, "globalCoordinateSystemComboBox")
            verify(picker !== null, "globalCoordinateSystemComboBox must exist")
            picker.committed(cs)
            tryCompare(RootData.region, "globalCoordinateSystem", cs)
        }

        function setInputCSViaPicker(rowIndex, cs) {
            const fixPage = RootData.pageView.currentPageItem
            let picker = null
            tryVerify(() => {
                picker = findChild(fixPage, "inputCSComboBox." + rowIndex)
                return picker !== null
            }, 5000, "row " + rowIndex + " inputCSComboBox should be reachable")
            picker.committed(cs)
        }

        function test_globalCSAndFixesRoundTrip() {
            setGlobalCSViaPicker("EPSG:32612")

            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = "RoundTripCave"

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + String(cave.name) + "/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000)

            const addButton = findChild(findChild(RootData.pageView.currentPageItem, "addFixBar"), "addButton")
            mouseClick(addButton)
            mouseClick(addButton)
            tryCompare(cave.fixStations, "count", 2)

            const m = cave.fixStations
            m.setData(m.index(0), "A1", FixStationModel.StationNameRole)
            m.setData(m.index(0), 500000.0, FixStationModel.EastingRole)
            m.setData(m.index(0), 4194000.0, FixStationModel.NorthingRole)
            m.setData(m.index(0), 2700.0, FixStationModel.ElevationRole)
            setInputCSViaPicker(0, "EPSG:4326")

            m.setData(m.index(1), "B1", FixStationModel.StationNameRole)
            m.setData(m.index(1), 500200.0, FixStationModel.EastingRole)
            m.setData(m.index(1), 4194100.0, FixStationModel.NorthingRole)
            m.setData(m.index(1), 2750.0, FixStationModel.ElevationRole)
            setInputCSViaPicker(1, "EPSG:32612")

            // cwFixStation is a value type, so read through the model role API.
            tryVerify(() => m.data(m.index(0), FixStationModel.StationNameRole) === "A1")
            tryVerify(() => m.data(m.index(0), FixStationModel.InputCSRole) === "EPSG:4326")
            tryVerify(() => m.data(m.index(1), FixStationModel.StationNameRole) === "B1")
            tryVerify(() => m.data(m.index(1), FixStationModel.InputCSRole) === "EPSG:32612")

            // tempDirectoryUrl() is PID-suffixed so parallel test processes don't collide.
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            const projectPath = tempDir + "/fix-stations-round-trip.cw"
            verify(RootData.project.saveAs(projectPath), "saveAs should succeed")
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            RootData.newProject()
            tryCompare(RootData.region, "caveCount", 0)

            RootData.project.loadFile(TestHelper.toLocalUrl(projectPath))
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            tryVerify(() => RootData.region.caveCount === 1, 10000,
                      "reloaded project should contain the round-trip cave")

            tryCompare(RootData.region, "globalCoordinateSystem", "EPSG:32612")

            const reloadedModel = RootData.region.cave(0).fixStations
            tryCompare(reloadedModel, "count", 2)

            const r0 = reloadedModel.index(0)
            compare(reloadedModel.data(r0, FixStationModel.StationNameRole), "A1")
            compare(reloadedModel.data(r0, FixStationModel.InputCSRole), "EPSG:4326")
            compare(reloadedModel.data(r0, FixStationModel.EastingRole), 500000.0)
            compare(reloadedModel.data(r0, FixStationModel.NorthingRole), 4194000.0)
            compare(reloadedModel.data(r0, FixStationModel.ElevationRole), 2700.0)

            const r1 = reloadedModel.index(1)
            compare(reloadedModel.data(r1, FixStationModel.StationNameRole), "B1")
            compare(reloadedModel.data(r1, FixStationModel.InputCSRole), "EPSG:32612")
            compare(reloadedModel.data(r1, FixStationModel.EastingRole), 500200.0)
            compare(reloadedModel.data(r1, FixStationModel.NorthingRole), 4194100.0)
            compare(reloadedModel.data(r1, FixStationModel.ElevationRole), 2750.0)

            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000)
            const reloadedPicker = findChild(RootData.pageView.currentPageItem, "globalCoordinateSystemComboBox")
            tryCompare(reloadedPicker, "value", "EPSG:32612")
        }
    }
}
