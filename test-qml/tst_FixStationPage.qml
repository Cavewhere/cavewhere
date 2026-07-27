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

        function test_domainErrorTintsTheCoordinateCell() {
            // U4's point is the *cell*: the role-level attribution is covered in
            // C++, so what only QML can prove is that a coordinate outside its
            // CS actually turns red and back. Since U11 put all three components
            // in one field, either per-axis flag has to reach that one tint —
            // both directions are checked, because an `||` that lost a term
            // would still pass with only one of them.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const idx = model.index(0)
            model.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            model.setData(idx, 478000.0, FixStationModel.EastingRole)
            model.setData(idx, 4430000.0, FixStationModel.NorthingRole)

            const fixPage = RootData.pageView.currentPageItem
            let coordinateCell = null
            tryVerify(() => {
                coordinateCell = findChild(fixPage, "coordinateCell.0")
                return coordinateCell !== null
            }, 5000, "row 0 coordinate cell should be reachable")

            verify(!coordinateCell.error, "a clean coordinate carries no error flag")
            compare(coordinateCell.color, Theme.text,
                    "a clean coordinate uses the normal text color")

            const diagnostics = cave.fixStationDiagnostics
            const diagnosticsIdx = diagnostics.index(0)

            // Transposed leading digit in the easting only.
            model.setData(idx, 1478000.0, FixStationModel.EastingRole)
            tryVerify(() => coordinateCell.error, 5000, "the bad easting is flagged")
            compare(coordinateCell.color, Theme.errorText, "the bad easting is tinted red")
            verify(!diagnostics.data(diagnosticsIdx,
                                     FixStationDiagnosticsModel.NorthingDomainErrorRole),
                   "the northing stays unflagged, so only the easting term can be lighting it")

            model.setData(idx, 478000.0, FixStationModel.EastingRole)
            tryVerify(() => !coordinateCell.error, 5000, "correcting the easting clears the flag")
            compare(coordinateCell.color, Theme.text, "and restores the normal text color")

            // ...and again with the northing out of domain instead.
            model.setData(idx, -2000000.0, FixStationModel.NorthingRole)
            tryVerify(() => coordinateCell.error, 5000, "the bad northing is flagged too")
            verify(!diagnostics.data(diagnosticsIdx,
                                     FixStationDiagnosticsModel.EastingDomainErrorRole),
                   "the easting stays unflagged, so only the northing term can be lighting it")

            model.setData(idx, 4430000.0, FixStationModel.NorthingRole)
            tryVerify(() => !coordinateCell.error, 5000, "correcting the northing clears it")
        }

        function test_coordinateCellShowsTheWholeCoordinate() {
            // The one field is both the display and the input (#621), so what it
            // renders has to be text the parser reads back unchanged — otherwise
            // opening the editor and pressing Enter would drift the fix.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const idx = model.index(0)
            // No input CS, so the row falls back to the region's global one and
            // is written easting first.
            model.setData(idx, 46.12113, FixStationModel.EastingRole)
            model.setData(idx, -115.59902, FixStationModel.NorthingRole)
            model.setData(idx, 304.0, FixStationModel.ElevationRole)

            const fixPage = RootData.pageView.currentPageItem
            let coordinateCell = null
            tryVerify(() => {
                coordinateCell = findChild(fixPage, "coordinateCell.0")
                return coordinateCell !== null
            }, 5000, "row 0 coordinate cell should be reachable")

            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 304m", 5000,
                       "the cell spells out the whole coordinate with its elevation unit")

            // A component edited elsewhere has to move the field with it.
            model.setData(idx, 305.0, FixStationModel.ElevationRole)
            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 305m", 5000,
                       "the cell tracks the model rather than caching what it drew")

            // Switching the project to imperial has to move both the number and
            // the suffix: a bare elevation is read back in the project's units,
            // so a field that kept saying "305" would now mean 305 ft.
            // The long decimal is the price of an exact round trip: the field is
            // its own input, so a converted elevation is rendered to whatever it
            // takes to read back as the same value.
            const previousUnits = RootData.region.unitSystem
            RootData.region.unitSystem = Units.Imperial
            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 1000.6561679790026ft",
                       5000, "the elevation follows the project's unit system, suffix and all")

            RootData.region.unitSystem = previousUnits
            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 305m", 5000)
        }

        function test_coordinateCellCommitsAWholeCoordinate() {
            // The paste path, end to end through the page: one string in, three
            // model roles out, with the ft suffix converted on the way.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const fixPage = RootData.pageView.currentPageItem
            tryVerify(() => findChild(fixPage, "coordinateCell.0") !== null, 5000,
                      "row 0 coordinate cell should be reachable")

            fixPage.commitCoordinate(0, "46.12113, -115.59902, 304ft")

            const model = cave.fixStations
            const idx = model.index(0)
            fuzzyCompare(model.data(idx, FixStationModel.EastingRole), 46.12113, 1e-9)
            fuzzyCompare(model.data(idx, FixStationModel.NorthingRole), -115.59902, 1e-9)
            fuzzyCompare(model.data(idx, FixStationModel.ElevationRole), 304 * 0.3048, 1e-9)
        }

        function test_coordinateCellWritesLatitudeFirstForAGeographicCS() {
            // #621 writes lat/long the way people do — latitude first — but the
            // model stores easting/northing, so a geographic row has to render
            // its two horizontal components the other way round from a
            // projected one. Getting this backwards transposes the fix.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const idx = model.index(0)
            model.setData(idx, "EPSG:4326", FixStationModel.InputCSRole)
            model.setData(idx, -115.59902, FixStationModel.EastingRole)   // longitude
            model.setData(idx, 46.12113, FixStationModel.NorthingRole)    // latitude
            model.setData(idx, 304.0, FixStationModel.ElevationRole)

            const fixPage = RootData.pageView.currentPageItem
            let coordinateCell = null
            tryVerify(() => {
                coordinateCell = findChild(fixPage, "coordinateCell.0")
                return coordinateCell !== null
            }, 5000, "row 0 coordinate cell should be reachable")

            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 304m", 5000,
                       "a geographic row leads with the latitude")

            // Switching to a projected CS swaps the two on screen, with the
            // stored coordinate untouched.
            model.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            tryCompare(coordinateCell, "text", "-115.59902, 46.12113, 304m", 5000,
                       "a projected row leads with the easting")
            compare(model.data(idx, FixStationModel.EastingRole), -115.59902,
                    "and nothing was written to the model by re-rendering")
        }

        function test_coordinateCellCommitsLatitudeFirstForAGeographicCS() {
            // The commit half of the same rule. parse() and format() have to
            // agree on the order or a value would transpose itself on every
            // trip through the field.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const idx = model.index(0)
            model.setData(idx, "EPSG:4326", FixStationModel.InputCSRole)

            const fixPage = RootData.pageView.currentPageItem
            tryVerify(() => findChild(fixPage, "coordinateCell.0") !== null, 5000,
                      "row 0 coordinate cell should be reachable")

            fixPage.commitCoordinate(0, "46.12113, -115.59902, 304m")

            fuzzyCompare(model.data(idx, FixStationModel.NorthingRole), 46.12113, 1e-9)
            fuzzyCompare(model.data(idx, FixStationModel.EastingRole), -115.59902, 1e-9)

            // Round trip: what the cell now shows must commit back unchanged.
            const coordinateCell = findChild(fixPage, "coordinateCell.0")
            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 304m", 5000)
            fixPage.commitCoordinate(0, coordinateCell.text)
            fuzzyCompare(model.data(idx, FixStationModel.NorthingRole), 46.12113, 1e-9)
            fuzzyCompare(model.data(idx, FixStationModel.EastingRole), -115.59902, 1e-9)
        }

        function test_coordinateValidatorHoldsBackTextItCantRead() {
            // #621 asks for a validator. It must never report Invalid — that
            // rejects the keystroke and no coordinate is complete while it's
            // being typed — so partial text is Intermediate, which
            // CoreClickTextInput refuses to commit while showing the reason.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const fixPage = RootData.pageView.currentPageItem
            let coordinateCell = null
            tryVerify(() => {
                coordinateCell = findChild(fixPage, "coordinateCell.0")
                return coordinateCell !== null
            }, 5000, "row 0 coordinate cell should be reachable")

            const validator = coordinateCell.validator
            verify(validator !== null, "the coordinate cell carries a validator")

            // Only the coordinate cell. A validator installed on every FixField
            // would satisfy the line above just as well, and would then refuse
            // every station name that isn't a coordinate.
            const stationCell = findChild(fixPage, "stationCell.0")
            verify(stationCell !== null, "row 0 station cell should be reachable")
            compare(stationCell.validator, null, "a station name is not a coordinate")

            const acceptable = 2
            const intermediate = 1
            compare(validator.validate("46.12113, -115.59902, 304m"), acceptable,
                    "a whole coordinate is acceptable")
            compare(validator.validate("46.12113,"), intermediate,
                    "half-typed text is held back, not rejected")
            compare(validator.validate("46.12113, -115.59902, 304 bananas"), intermediate)
            verify(validator.errorText.indexOf("bananas") >= 0,
                   "and the reason names what it couldn't read")

            // Asking the validator directly says nothing about what the field
            // does with the answer, and "held back" is entirely
            // CoreClickTextInput.commitChanges()'s doing — so drive the real
            // path: open the editor, type into the shadow input, and commit.
            const model = cave.fixStations
            const idx = model.index(0)
            coordinateCell.openEditor()
            GlobalShadowTextInput.textInput.text = "46.12113, -115.59902, 304 bananas"
            compare(coordinateCell.commitChanges(), false, "the commit is refused")
            verify(coordinateCell.isEditting, "and the editor stays open on the bad text")
            compare(GlobalShadowTextInput.errorHelpBox.text, validator.errorText,
                    "with the reason shown where the user is typing")
            compare(model.data(idx, FixStationModel.EastingRole), 0,
                    "nothing reached the model")

            // The row declares no CS, so the field reads easting first.
            GlobalShadowTextInput.textInput.text = "610016.792, 5615117.075, 304m"
            compare(coordinateCell.commitChanges(), true, "correcting it commits")
            verify(!coordinateCell.isEditting, "and closes the editor")
            fuzzyCompare(model.data(idx, FixStationModel.EastingRole), 610016.792, 1e-6)
            fuzzyCompare(model.data(idx, FixStationModel.NorthingRole), 5615117.075, 1e-6)
        }

        function test_coordinateValidatorExplainsItselfInTheRowsAxisOrder() {
            // The validator is the only message this page ever shows, so it has
            // to carry the row's own order: a geographic row told to type "an
            // easting and a northing", with a UTM worked example, is worse off
            // than with no message at all.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const idx = model.index(0)

            const fixPage = RootData.pageView.currentPageItem
            let coordinateCell = null
            tryVerify(() => {
                coordinateCell = findChild(fixPage, "coordinateCell.0")
                return coordinateCell !== null
            }, 5000, "row 0 coordinate cell should be reachable")

            // No CS yet: the row falls back to easting-first.
            tryCompare(coordinateCell, "axisOrder", CoordinateText.EastingNorthing, 5000)
            coordinateCell.validator.validate("46.12113")
            verify(coordinateCell.validator.errorText.indexOf("easting") >= 0,
                   "a projected row is told about an easting")

            model.setData(idx, "EPSG:4326", FixStationModel.InputCSRole)
            tryCompare(coordinateCell, "axisOrder", CoordinateText.LatitudeLongitude, 5000,
                       "the field follows its own row's CS")

            coordinateCell.validator.validate("46.12113")
            verify(coordinateCell.validator.errorText.indexOf("latitude") >= 0,
                   "and a geographic row is told about a latitude")
            verify(coordinateCell.validator.errorText.indexOf("easting") < 0,
                   "with no mention of the axis it doesn't have")
        }

        function test_coordinateCellCommitsThroughTheFieldItself() {
            // The other commit tests call commitCoordinate() directly, which
            // leaves FixField.onFinishedEditting untested — and that handler is
            // the only thing routing a coordinate cell past commitEdit(). Its
            // own signal is what makes the branch load-bearing.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const fixPage = RootData.pageView.currentPageItem
            let coordinateCell = null
            tryVerify(() => {
                coordinateCell = findChild(fixPage, "coordinateCell.0")
                return coordinateCell !== null
            }, 5000, "row 0 coordinate cell should be reachable")

            coordinateCell.finishedEditting("610016.792, 5615117.075, 2545.34m")

            const model = cave.fixStations
            const idx = model.index(0)
            fuzzyCompare(model.data(idx, FixStationModel.EastingRole), 610016.792, 1e-6,
                         "the field's own commit reaches setCoordinateText")
            fuzzyCompare(model.data(idx, FixStationModel.NorthingRole), 5615117.075, 1e-6)
            fuzzyCompare(model.data(idx, FixStationModel.ElevationRole), 2545.34, 1e-6)
        }

        function test_coordinateCellReadsABareElevationInProjectUnits() {
            // Every other commit string here carries an explicit m or ft, which
            // overrides the unit system — so nothing yet proved ProjectUnits
            // reaches the parse side. A bare elevation is the only input that
            // can tell the difference.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const fixPage = RootData.pageView.currentPageItem
            tryVerify(() => findChild(fixPage, "coordinateCell.0") !== null, 5000,
                      "row 0 coordinate cell should be reachable")

            const previousUnits = RootData.region.unitSystem
            RootData.region.unitSystem = Units.Imperial
            fixPage.commitCoordinate(0, "478000, 4430000, 5430")

            const model = cave.fixStations
            fuzzyCompare(model.data(model.index(0), FixStationModel.ElevationRole),
                         5430 * 0.3048, 1e-6,
                         "a bare elevation is read as feet in an imperial project")

            RootData.region.unitSystem = previousUnits
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
