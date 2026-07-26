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

            // addFixBar is positioned by a LayoutItemProxy; wait for layout so the
            // click lands on the button rather than its stale (0,0) position.
            waitForRendering(rootId)
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

        function test_domainErrorRoleFlagsBadCoordinate() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            // Edits go to the row model; the warning is read off the
            // diagnostics proxy, which is where the derived roles live.
            const model = cave.fixStations
            const diagnostics = cave.fixStationDiagnostics
            const idx = model.index(0)
            const proxyIdx = diagnostics.index(0)
            model.setData(idx, "BAD", FixStationModel.StationNameRole)
            model.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            // A transposed leading digit (1478000 easting) lands outside UTM
            // 13N's valid domain, so the per-row check flags it on its own.
            model.setData(idx, 1478000.0, FixStationModel.EastingRole)
            model.setData(idx, 4430000.0, FixStationModel.NorthingRole)

            const err = diagnostics.data(proxyIdx, FixStationDiagnosticsModel.DomainErrorRole)
            verify(err.length > 0, "domain error should be set for an out-of-range easting")
            verify(err.indexOf("outside the valid range") >= 0, "message: " + err)

            // Correcting the coordinate clears the role.
            model.setData(idx, 478000.0, FixStationModel.EastingRole)
            compare(diagnostics.data(proxyIdx, FixStationDiagnosticsModel.DomainErrorRole), "",
                    "domain error clears once the coordinate is in range")
        }

        function test_domainErrorRoleQuietForGoodCoordinate() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const diagnostics = cave.fixStationDiagnostics
            const idx = model.index(0)
            const proxyIdx = diagnostics.index(0)
            model.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            model.setData(idx, 478000.0, FixStationModel.EastingRole)
            model.setData(idx, 4430000.0, FixStationModel.NorthingRole)
            compare(diagnostics.data(proxyIdx, FixStationDiagnosticsModel.DomainErrorRole), "",
                    "an in-range coordinate raises no domain error")

            // No input CS to judge against → the row never flags on its own.
            model.setData(idx, "", FixStationModel.InputCSRole)
            model.setData(idx, 1478000.0, FixStationModel.EastingRole)
            compare(diagnostics.data(proxyIdx, FixStationDiagnosticsModel.DomainErrorRole), "",
                    "a blank input CS defers to the region-level check")
        }

        function test_domainWarningIconShowsInline() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const idx = model.index(0)
            model.setData(idx, "BAD", FixStationModel.StationNameRole)
            model.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            model.setData(idx, 478000.0, FixStationModel.EastingRole)
            model.setData(idx, 4430000.0, FixStationModel.NorthingRole)

            const fixPage = RootData.pageView.currentPageItem
            let warning = null
            tryVerify(() => {
                warning = findChild(fixPage, "domainWarning.0")
                return warning !== null
            }, 5000, "row 0 domain warning indicator should be reachable")
            verify(!warning.visible, "warning is hidden for a well-formed row")

            model.setData(idx, 1478000.0, FixStationModel.EastingRole)
            tryVerify(() => warning.visible, 2000,
                      "warning appears once the coordinate leaves its CS domain")

            model.setData(idx, 478000.0, FixStationModel.EastingRole)
            tryVerify(() => !warning.visible, 2000,
                      "warning clears once the coordinate is corrected")
        }

        function test_domainErrorTintsTheOffendingCell() {
            // U4's point is the *cell*: the role-level attribution is covered in
            // C++, so what only QML can prove is that the offending coordinate
            // actually turns red and back. Guards the FixField.error binding and
            // the WideCell alias chain, either of which could be dropped without
            // failing any other suite.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const idx = model.index(0)
            model.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            model.setData(idx, 478000.0, FixStationModel.EastingRole)
            model.setData(idx, 4430000.0, FixStationModel.NorthingRole)

            const fixPage = RootData.pageView.currentPageItem
            let eastingCell = null
            tryVerify(() => {
                eastingCell = findChild(fixPage, "eastingCell.0")
                return eastingCell !== null
            }, 5000, "row 0 easting cell should be reachable")

            verify(!eastingCell.error, "a clean easting carries no error flag")
            compare(eastingCell.color, Theme.text, "a clean easting uses the normal text color")

            // Transposed leading digit in the easting only.
            model.setData(idx, 1478000.0, FixStationModel.EastingRole)
            tryVerify(() => eastingCell.error, 5000, "the bad easting is flagged")
            compare(eastingCell.color, Theme.errorText, "the bad easting is tinted red")
            const diagnostics = cave.fixStationDiagnostics
            verify(!diagnostics.data(diagnostics.index(0, 0),
                                     FixStationDiagnosticsModel.NorthingDomainErrorRole),
                   "the northing stays unflagged")

            // Correcting the coordinate clears the tint again.
            model.setData(idx, 478000.0, FixStationModel.EastingRole)
            tryVerify(() => !eastingCell.error, 5000, "correcting the easting clears the flag")
            compare(eastingCell.color, Theme.text, "and restores the normal text color")
        }

        function test_stationWarningIconHiddenWithoutSurvey() {
            // Without a computed survey network there's nothing to validate the
            // station reference against, so the icon stays hidden — and its
            // objectName confirms the shared InlineWarning wiring is intact.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            model.setData(model.index(0), "A1", FixStationModel.StationNameRole)

            const fixPage = RootData.pageView.currentPageItem
            let warning = null
            tryVerify(() => {
                warning = findChild(fixPage, "stationWarning.0")
                return warning !== null
            }, 5000, "row 0 station warning indicator should be reachable")
            verify(!warning.visible, "no survey network → no station-reference warning")
        }

        function test_stationWarningShownForEmptyName() {
            // A blank row names no survey station — survex drops such a fix — so
            // it is flagged inline the moment it appears, even before a network
            // exists (the empty check runs before the "nothing to check" defer).
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const diagnostics = cave.fixStationDiagnostics
            verify(diagnostics.data(diagnostics.index(0),
                                    FixStationDiagnosticsModel.StationErrorRole) !== "",
                   "an empty station name is flagged")

            const fixPage = RootData.pageView.currentPageItem
            let warning = null
            tryVerify(() => {
                warning = findChild(fixPage, "stationWarning.0")
                return warning !== null
            }, 5000, "row 0 station warning indicator should be reachable")
            verify(warning.visible, "empty station name → station-reference warning shows")

            // Typing any name clears it: with no network there is nothing to
            // match against, so a *named* fix defers rather than cry wolf. The
            // flag is specific to the name being missing altogether.
            model.setData(model.index(0), "A1", FixStationModel.StationNameRole)
            tryVerify(() => !warning.visible, 5000,
                      "a named fix with no network defers and clears the warning")
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
