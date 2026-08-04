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

        function setInputCSViaPicker(rowIndex, cs) {
            const fixPage = RootData.pageView.currentPageItem
            let picker = null
            tryVerify(() => {
                picker = findChild(fixPage, "inputCSComboBox." + rowIndex)
                return picker !== null
            }, 5000, "row " + rowIndex + " inputCSComboBox should be reachable")
            picker.committed(cs)
        }

        function test_theFrameAndFixesRoundTrip() {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = "RoundTripCave"

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + String(cave.name) + "/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000)

            const addButton = findChild(findChild(RootData.pageView.currentPageItem, "addFixBar"), "addButton")
            // addFixBar is positioned by a LayoutItemProxy; wait for layout so the
            // clicks land on the button rather than its stale (0,0) position.
            waitForRendering(rootId)
            mouseClick(addButton)
            mouseClick(addButton)
            tryCompare(cave.fixStations, "count", 2)

            // The CS goes in before the components, always: it decides which
            // axis the coordinate leads with, so writing numbers first spells
            // them out easting-first and a geographic CS then re-reads them
            // latitude-first, swapping the two. Both importers do it in this
            // order for the same reason (see cwFixStation::setInputCS).
            const m = cave.fixStations
            m.setData(m.index(0), "A1", FixStationModel.StationNameRole)
            setInputCSViaPicker(0, "EPSG:4326")
            m.setData(m.index(0), 500000.0, FixStationModel.EastingRole)
            m.setData(m.index(0), 4194000.0, FixStationModel.NorthingRole)
            m.setData(m.index(0), 2700.0, FixStationModel.ElevationRole)

            m.setData(m.index(1), "B1", FixStationModel.StationNameRole)
            setInputCSViaPicker(1, "EPSG:32612")
            m.setData(m.index(1), 500200.0, FixStationModel.EastingRole)
            m.setData(m.index(1), 4194100.0, FixStationModel.NorthingRole)
            m.setData(m.index(1), 2750.0, FixStationModel.ElevationRole)

            // cwFixStation is a value type, so read through the model role API.
            tryVerify(() => m.data(m.index(0), FixStationModel.StationNameRole) === "A1")
            tryVerify(() => m.data(m.index(0), FixStationModel.InputCSRole) === "EPSG:4326")
            tryVerify(() => m.data(m.index(1), FixStationModel.StationNameRole) === "B1")
            tryVerify(() => m.data(m.index(1), FixStationModel.InputCSRole) === "EPSG:32612")

            const frameBeforeSave = RootData.region.geoReference.localCoordinateSystem
            verify(frameBeforeSave.length > 0, "the first fix must have anchored a frame")

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

            // The frame the first fix anchored comes back with the file, on the
            // same anchor — a reload that re-derived it would be a different
            // projection and every station would land somewhere else.
            verify(RootData.region.geoReference.hasCoordinateSystem,
                   "the reloaded project must still carry its frame")
            compare(RootData.region.geoReference.state, GeoReference.Anchored)
            compare(RootData.region.geoReference.localCoordinateSystem, frameBeforeSave)
        }
    }
}
