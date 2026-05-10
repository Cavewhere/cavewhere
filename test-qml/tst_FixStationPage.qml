import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    TestCase {
        name: "FixStationPage"
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
                      5000, "should be on view page at start of test")
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        function gotoFixStations() {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = "PR4Cave"

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + String(cave.name) + "/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should land on fixStationPage")
            return cave
        }

        function findPicker(rowIndex) {
            const fixPage = RootData.pageView.currentPageItem
            return findChild(fixPage, "inputCSComboBox." + rowIndex)
        }

        function test_emptyStateShowsHelpBox() {
            gotoFixStations()

            const fixPage = RootData.pageView.currentPageItem
            const help = findChild(fixPage, "noFixStationsHelpBox")
            verify(help !== null, "noFixStationsHelpBox must exist")
            tryVerify(() => help.visible, 2000, "help box should be visible for empty fix list")

            const tableView = findChild(fixPage, "fixStationTableView")
            verify(tableView !== null, "fixStationTableView must exist")
            tryCompare(tableView, "count", 0)
        }

        function test_addFixViaButton() {
            const cave = gotoFixStations()

            const fixPage = RootData.pageView.currentPageItem
            const addBar = findChild(fixPage, "addFixBar")
            verify(addBar !== null, "addFixBar must exist")
            const addButton = findChild(addBar, "addButton")
            verify(addButton !== null, "addButton must exist")

            mouseClick(addButton)
            tryCompare(cave.fixStations, "count", 1)

            const tableView = findChild(fixPage, "fixStationTableView")
            tryCompare(tableView, "count", 1)

            const help = findChild(fixPage, "noFixStationsHelpBox")
            tryVerify(() => !help.visible, 2000, "help box should hide once a fix is added")
        }

        function test_editCellsThroughModel() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const idx = model.index(0)
            model.setData(idx, "A1", FixStationModel.StationNameRole)
            model.setData(idx, 500200.0, FixStationModel.EastingRole)
            model.setData(idx, 4194100.0, FixStationModel.NorthingRole)
            model.setData(idx, 2750.5, FixStationModel.ElevationRole)

            // cwFixStation is a plain value type (not Q_GADGET), so its
            // accessors aren't reachable from QML — read fields back through
            // the model's role API instead.
            compare(model.data(idx, FixStationModel.StationNameRole), "A1")
            compare(model.data(idx, FixStationModel.EastingRole), 500200.0)
            compare(model.data(idx, FixStationModel.NorthingRole), 4194100.0)
            compare(model.data(idx, FixStationModel.ElevationRole), 2750.5)
        }

        function test_editInputCSViaPicker() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            // Wait for the row delegate (and its CSComboBox) to instantiate.
            let picker = null
            tryVerify(() => {
                picker = findPicker(0)
                return picker !== null
            }, 5000, "row 0 inputCSComboBox should be reachable")

            // The picker emits committed(newCS) from any of its modes
            // (Local / Lat-Lon / UTM / Custom). Driving the signal exercises
            // the same handler that mouse interaction would.
            const m = cave.fixStations
            const idx = m.index(0)

            picker.committed("EPSG:32612")
            tryVerify(() => m.data(idx, FixStationModel.InputCSRole) === "EPSG:32612")

            picker.committed("EPSG:4326")
            tryVerify(() => m.data(idx, FixStationModel.InputCSRole) === "EPSG:4326")

            picker.committed("")
            tryVerify(() => m.data(idx, FixStationModel.InputCSRole) === "")
        }

        function test_inputCSPickerAllowsGeographic() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            let picker = null
            tryVerify(() => {
                picker = findPicker(0)
                return picker !== null
            }, 5000, "row 0 inputCSComboBox should be reachable")

            // allowGeographic defaults to true on FixStationPage rows so the
            // user can mark a fix in raw lat/lon (EPSG:4326). The picker's
            // mode list must therefore include LatLon.
            verify(picker.allowGeographic, "FixStationPage CSComboBox should allow geographic")
            const modeCombo = findChild(picker, "csModePicker")
            verify(modeCombo !== null, "csModePicker should be reachable")
            compare(modeCombo.model.length, 4,
                    "Local / Lat-Lon / UTM / Custom (4 modes) when allowGeographic")
        }

        function test_removeFixConfirmed() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            verify(cave.fixStations.setData(cave.fixStations.index(0), "A1",
                                            FixStationModel.StationNameRole))
            tryCompare(cave.fixStations, "count", 1)

            const fixPage = RootData.pageView.currentPageItem
            const removeAskBox = findChild(fixPage, "removeChallange")
            verify(removeAskBox !== null, "RemoveAskBox must exist on fixStationPage")

            removeAskBox.indexToRemove = 0
            removeAskBox.removeName = "A1"
            removeAskBox.show()
            tryVerify(() => removeAskBox.visible)

            const removeButton = findChild(removeAskBox, "removeButton")
            mouseClick(removeButton)
            tryVerify(() => !removeAskBox.visible)

            tryCompare(cave.fixStations, "count", 0)

            const help = findChild(fixPage, "noFixStationsHelpBox")
            tryVerify(() => help.visible, 2000, "help box returns when last fix removed")
        }

        function test_removeFixCancelled() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const fixPage = RootData.pageView.currentPageItem
            const removeAskBox = findChild(fixPage, "removeChallange")
            removeAskBox.indexToRemove = 0
            removeAskBox.removeName = "A1"
            removeAskBox.show()
            tryVerify(() => removeAskBox.visible)

            mouseClick(findChild(removeAskBox, "cancelButton"))
            tryVerify(() => !removeAskBox.visible)

            compare(cave.fixStations.count, 1)
        }

        function test_fixStationsLinkCount() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            cave.fixStations.addFixStation()

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + String(cave.name)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      5000, "should land on cavePage")

            const link = findChild(RootData.pageView.currentPageItem, "fixStationsLink")
            tryCompare(link, "text", "2")

            cave.fixStations.removeAt(0)
            tryCompare(link, "text", "1")
        }
    }
}
