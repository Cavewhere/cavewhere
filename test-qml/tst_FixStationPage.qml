import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    TestCase {
        name: "FixStationPage"
        when: windowShown

        // Captured once, not bound: a binding to rootId.width would follow the
        // window when a test narrows it, and "restore the default" would then
        // restore the narrow width.
        property int defaultWindowWidth: 0

        Component.onCompleted: defaultWindowWidth = rootId.width

        // Retargeted per test rather than bound: the model belongs to a cave
        // that init() throws away.
        SignalSpy {
            id: dataChangedSpyId
            signalName: "dataChanged"
        }

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
            // Restored here as well as in the test that narrows it: QtTest runs
            // functions alphabetically, so a width left behind would silently
            // put every later test in the other layout.
            rootId.width = defaultWindowWidth
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

        //! Add a row on a projected system, so its coordinate leads with the
        //! easting. New rows start on WGS84, where the same text would be read
        //! latitude first.
        function addProjectedFixStation(cave) {
            cave.fixStations.addFixStation()
            const row = cave.fixStations.count - 1
            cave.fixStations.setData(cave.fixStations.index(row), "EPSG:32611",
                                     FixStationModel.InputCSRole)
            return row
        }

        //! The datum a frame derived in the conterminous US is plate-fixed to,
        //! so a project anchored there has a context datum that is not the
        //! WGS84 fallback.
        readonly property string nad83Datum: "EPSG:6318"

        //! Lands on the fix page with row 0 anchoring the project in Kentucky,
        //! giving the region a frame and a default fix datum of its own.
        function anchorProjectOnNad83() {
            const cave = gotoFixStations()
            const model = cave.fixStations

            // The system goes in before the numbers — it decides which axis the
            // coordinate leads with.
            compare(model.addFixStation("A1"), 0)
            model.setData(model.index(0), "EPSG:4326", FixStationModel.InputCSRole)
            model.setCoordinateText(0, "37.0, -84.0, 300m", Units.Metric)

            tryCompare(RootData.region, "defaultFixDatum", nad83Datum, 5000)
            return cave
        }

        //! The datum combo's row index for the short name \a datumName. The rows
        //! read "<region> · <name>", so the short name alone matches none.
        function datumRowFor(datumCombo, datumName) {
            return datumCombo.model.findIndex((label) => label.endsWith(" · " + datumName))
        }

        //! Row \a rowIndex's datum combo.
        function datumComboFor(rowIndex) {
            const combo = findChild(waitForPicker(rowIndex), "csDatum")
            verify(combo !== null, "csDatum should be reachable")
            return combo
        }

        //! The tooltip beside row \a rowIndex's datum combo.
        function datumToolTipFor(rowIndex) {
            const toolTip = findChild(waitForPicker(rowIndex), "csDatumToolTip")
            verify(toolTip !== null, "csDatumToolTip should be reachable")
            return toolTip
        }

        //! The datum codes the combo is currently offering, short names.
        function datumNamesOf(datumCombo) {
            return datumCombo.model.map((label) => label.split(" \u00b7 ")[1])
        }

        function findPicker(rowIndex) {
            const fixPage = RootData.pageView.currentPageItem
            return findChild(fixPage, "inputCSComboBox." + rowIndex)
        }

        //! Row \a rowIndex's coordinate-system picker, once the delegate
        //! carrying it exists — the row is added to the model before the view
        //! has built it.
        function waitForPicker(rowIndex) {
            let picker = null
            tryVerify(() => {
                picker = findPicker(rowIndex)
                return picker !== null
            }, 5000, "row " + rowIndex + " inputCSComboBox should be reachable")
            return picker
        }

        //! Row \a rowIndex's coordinate cell, once the delegate carrying it
        //! exists — the row is added to the model before the view has built it.
        function findCoordinateCell(rowIndex) {
            let cell = null
            tryVerify(() => {
                cell = findChild(RootData.pageView.currentPageItem,
                                 "coordinateCell." + rowIndex)
                return cell !== null
            }, 5000, "row " + rowIndex + " coordinate cell should be reachable")
            return cell
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

            // The picker emits committed(newCS) from any of its modes. Driving
            // the signal exercises the same handler that mouse interaction
            // would — including the blank an older file can still carry.
            const m = cave.fixStations
            const idx = m.index(0)

            picker.committed("EPSG:32612")
            tryVerify(() => m.data(idx, FixStationModel.InputCSRole) === "EPSG:32612")

            picker.committed("EPSG:4326")
            tryVerify(() => m.data(idx, FixStationModel.InputCSRole) === "EPSG:4326")

            picker.committed("")
            tryVerify(() => m.data(idx, FixStationModel.InputCSRole) === "")
        }

        function test_inputCSPickerModes() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            let picker = null
            tryVerify(() => {
                picker = findPicker(0)
                return picker !== null
            }, 5000, "row 0 inputCSComboBox should be reachable")

            // A fix station always has a coordinate system of its own, so
            // there is no "Local" here — that is what a blank input CS used to
            // mean, and it is what this whole change exists to retire.
            const modeCombo = findChild(picker, "csModePicker")
            verify(modeCombo !== null, "csModePicker should be reachable")
            verify(modeCombo.model.indexOf("Lat/Lon") >= 0,
                   "a fix-station row must offer a geographic CS")
            verify(modeCombo.model.indexOf("Lat/Lon (WGS84)") < 0,
                   "the mode must not name a datum — the datum combo does")
            verify(modeCombo.model.indexOf("Local") < 0,
                   "a fix-station row must not offer Local")
            compare(modeCombo.model.length, 3, "Lat/Lon, UTM and Custom")
        }

        //! The zone and hemisphere controls are the whole point of UTM mode:
        //! without them the zone is unreachable except through the Custom dialog.
        function test_inputCSPickerZoneIsEditableOnAUtmRow() {
            const cave = gotoFixStations()
            addProjectedFixStation(cave)
            tryCompare(cave.fixStations, "count", 1)

            let picker = null
            tryVerify(() => {
                picker = findPicker(0)
                return picker !== null
            }, 5000, "row 0 inputCSComboBox should be reachable")

            tryCompare(picker, "currentMode", CoordinateSystem.UTM)

            const zoneSpin = findChild(picker, "csUtmZone")
            verify(zoneSpin !== null, "csUtmZone should be reachable")
            tryVerify(() => zoneSpin.visible, 5000,
                      "the zone control must be visible on a UTM row")
            compare(zoneSpin.value, 11)

            zoneSpin.value = 14
            zoneSpin.valueModified()

            const inputCS = () => cave.fixStations.data(cave.fixStations.index(0),
                                                       FixStationModel.InputCSRole)
            tryVerify(() => inputCS() === "EPSG:32614", 5000,
                      "editing the zone must commit the new zone onto the row")
        }

        //! A row on a non-WGS84 UTM series keeps its datum when the zone moves:
        //! ETRS89 zone 32N to zone 33N stays ETRS89 (EPSG:25833), rather than
        //! landing on WGS84's EPSG:32633 a meter or two away.
        function test_inputCSPickerZoneEditKeepsTheDatum() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            cave.fixStations.setData(cave.fixStations.index(0), "EPSG:25832",
                                     FixStationModel.InputCSRole)
            tryCompare(cave.fixStations, "count", 1)

            const picker = waitForPicker(0)
            tryCompare(picker, "currentMode", CoordinateSystem.UTM)

            const zoneSpin = findChild(picker, "csUtmZone")
            verify(zoneSpin !== null, "csUtmZone should be reachable")
            compare(zoneSpin.value, 32)

            const inputCS = () => cave.fixStations.data(cave.fixStations.index(0),
                                                       FixStationModel.InputCSRole)

            zoneSpin.value = 33
            zoneSpin.valueModified()
            tryVerify(() => inputCS() === "EPSG:25833", 5000,
                      "the zone edit must stay on ETRS89")

            // Zone 60 is past the end of ETRS89's series, so the commit falls
            // back to WGS84 — the one series covering every zone.
            zoneSpin.value = 60
            zoneSpin.valueModified()
            tryVerify(() => inputCS() === "EPSG:32660", 5000,
                      "a zone outside the datum's series falls back to WGS84")
        }

        //! A row whose system names no datum is born WGS84 and stays there, even
        //! in a project whose own frame is plate-fixed to NAD83(2011). The datum
        //! reinterprets numbers the row already has, so it is the user's to
        //! change and nobody else's.
        function test_aRowWithNoDatumStaysOnWgs84() {
            const cave = anchorProjectOnNad83()
            const model = cave.fixStations

            // Row 1 carries numbers nobody could read an axis order for, which
            // is what a hand-edited file or an svx *fix with no *cs leaves.
            model.addFixStation()
            model.setData(model.index(1), "", FixStationModel.InputCSRole)
            tryCompare(model, "count", 2)

            const picker = waitForPicker(1)
            const modeCombo = findChild(picker, "csModePicker")
            verify(modeCombo !== null, "csModePicker should be reachable")

            modeCombo.activated(modeCombo.model.indexOf("Lat/Lon"))
            tryVerify(() => model.data(model.index(1), FixStationModel.InputCSRole)
                            === "EPSG:4326", 5000,
                      "naming Lat/Lon on a datum-less row commits WGS84")

            const datumCombo = findChild(picker, "csDatum")
            verify(datumCombo !== null, "csDatum should be reachable")
            tryVerify(() => datumCombo.visible, 5000, "Lat/Lon mode shows the datum")
            tryCompare(datumCombo, "displayText", "WGS84")
        }

        //! The datum is offered, never imposed: WGS84 stays one click away on a
        //! NAD83 row, which is what a coordinate off a phone needs.
        function test_theDatumComboCommitsWgs84OverNad83() {
            const cave = gotoFixStations()
            const model = cave.fixStations

            compare(model.addFixStation("A1"), 0)
            model.setData(model.index(0), nad83Datum, FixStationModel.InputCSRole)
            compare(model.setCoordinateText(0, "36.1, -85.5, 300m", Units.Metric), "")

            const datumCombo = datumComboFor(0)
            tryVerify(() => datumCombo.enabled, 5000,
                      "a row with a readable coordinate may change its datum")

            datumCombo.activated(datumRowFor(datumCombo, "WGS84"))
            tryVerify(() => model.data(model.index(0), FixStationModel.InputCSRole)
                            === "EPSG:4326", 5000,
                      "picking WGS84 commits the WGS84 geographic code")
        }

        //! UTM offers only the datums whose series reaches the zone on screen,
        //! and the row follows the same rule the list does.
        function test_theUtmDatumListFollowsTheZone() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            cave.fixStations.setData(cave.fixStations.index(0), "EPSG:25832",
                                     FixStationModel.InputCSRole)
            tryCompare(cave.fixStations, "count", 1)

            const picker = waitForPicker(0)
            const datumCombo = findChild(picker, "csDatum")
            verify(datumCombo !== null, "csDatum should be reachable")
            tryVerify(() => datumCombo.visible, 5000, "UTM mode shows the datum")
            tryVerify(() => datumRowFor(datumCombo, "ETRS89") >= 0, 5000,
                      "zone 32 is inside ETRS89's series")
            tryCompare(datumCombo, "displayText", "ETRS89")

            const zoneSpin = findChild(picker, "csUtmZone")
            zoneSpin.value = 60
            zoneSpin.valueModified()

            // ETRS89's series stops at zone 38, so it leaves both the list and
            // the row — the two agree on the WGS84 fallback.
            tryVerify(() => datumRowFor(datumCombo, "ETRS89") < 0, 5000,
                      "a zone outside a datum's series drops it from the list")
            tryCompare(datumCombo, "displayText", "WGS84")
            tryVerify(() => cave.fixStations.data(cave.fixStations.index(0),
                                                  FixStationModel.InputCSRole)
                            === "EPSG:32660", 5000)
        }

        //! An acronym like NAD83(2011) says nothing about where it applies, so
        //! the open list leads each row with the region while the closed control
        //! keeps the short name the narrow field was sized for.
        function test_theDatumRowsNameTheirRegion() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            cave.fixStations.setData(cave.fixStations.index(0), nad83Datum,
                                     FixStationModel.InputCSRole)
            tryCompare(cave.fixStations, "count", 1)

            const datumCombo = findChild(waitForPicker(0), "csDatum")
            verify(datumCombo !== null, "csDatum should be reachable")
            tryVerify(() => datumCombo.visible, 5000, "Lat/Lon mode shows the datum")
            tryCompare(datumCombo, "displayText", "NAD83(2011)")

            const nad83Row = datumRowFor(datumCombo, "NAD83(2011)")
            verify(nad83Row >= 0, "NAD83(2011) should be one of the rows")
            compare(datumCombo.model[nad83Row], "North America (USA) · NAD83(2011)")
            compare(datumCombo.model[datumRowFor(datumCombo, "WGS84")],
                    "World (GPS) · WGS84")

            datumCombo.popup.open()
            tryVerify(() => datumCombo.popup.opened, 5000, "the datum popup should open")

            let label = null
            tryVerify(() => {
                label = findChild(datumCombo.popup.contentItem,
                                  "csDatumItemLabel." + nad83Row)
                return label !== null
            }, 5000, "the NAD83 row's label should be reachable")

            compare(label.text, "North America (USA) · NAD83(2011)")
            tryVerify(() => !label.truncated, 5000,
                      "the popup must show a region-led row whole")
            verify(datumCombo.popup.width >= label.parent.width,
                   "the popup must be at least as wide as its rows")
            verify(datumCombo.popup.width > datumCombo.width,
                   "a region-led row needs more width than the closed field")

            datumCombo.popup.close()
            tryVerify(() => !datumCombo.popup.visible, 5000,
                      "the datum popup should close")
            tryCompare(datumCombo, "displayText", "NAD83(2011)")
        }

        //! A Custom CRS carries its datum inside itself, so there is nothing for
        //! the combo to pick and it stays out of the way.
        function test_customHidesTheDatumCombo() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const datumCombo = findChild(waitForPicker(0), "csDatum")
            verify(datumCombo !== null, "csDatum should be reachable")
            tryVerify(() => datumCombo.visible, 5000,
                      "a new row is on Lat/Lon, which shows the datum")

            cave.fixStations.setData(cave.fixStations.index(0), "EPSG:27700",
                                     FixStationModel.InputCSRole)
            tryVerify(() => !datumCombo.visible, 5000,
                      "Custom must hide the datum combo")
        }


        //! The datum says what existing numbers mean, so there is nothing for it
        //! to say before they are typed: a fresh row shows WGS84, locks the
        //! combo, and explains what to do first.
        function test_theDatumComboLocksUntilTheRowHasACoordinate() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const picker = waitForPicker(0)
            const datumCombo = datumComboFor(0)
            tryVerify(() => datumCombo.visible, 5000, "a new row is on Lat/Lon")
            tryVerify(() => !datumCombo.enabled, 5000,
                      "a row with no coordinate may not change its datum")
            tryCompare(datumCombo, "displayText", "WGS84")

            // Mode and zone stay live — only the datum waits on the coordinate.
            const modeCombo = findChild(picker, "csModePicker")
            verify(modeCombo.enabled, "the mode combo stays usable")

            const toolTip = datumToolTipFor(0)
            compare(toolTip.text, "Enter a coordinate to choose its datum")

            // A disabled control reports no hover of its own, so the tooltip
            // rides a handler on the wrapper around it.
            waitForRendering(rootId)
            mouseMove(datumCombo, datumCombo.width / 2, datumCombo.height / 2)
            tryVerify(() => toolTip.visible, 5000,
                      "hovering the locked datum combo should explain the lock")
        }

        //! A readable coordinate unlocks the datum and narrows the list to the
        //! frames that reach where it lands — WGS84 plus the plate-fixed one.
        function test_aReadableCoordinateUnlocksAndFiltersTheDatumList() {
            const cave = gotoFixStations()
            const model = cave.fixStations
            compare(model.addFixStation("A1"), 0)

            const datumCombo = datumComboFor(0)
            tryVerify(() => !datumCombo.enabled, 5000, "locked before the coordinate")

            compare(model.setCoordinateText(0, "36.1, -85.5, 300m", Units.Metric), "")

            tryVerify(() => datumCombo.enabled, 5000,
                      "a Tennessee coordinate unlocks the datum")
            tryVerify(() => datumCombo.model.length === 2, 5000,
                      "only WGS84 and the frame North America is fixed to")
            compare(datumCombo.model[0], "World (GPS) \u00b7 WGS84")
            compare(datumCombo.model[1], "North America (USA) \u00b7 NAD83(2011)")

            const toolTip = datumToolTipFor(0)
            verify(toolTip.text.indexOf("NAD83(2011)") >= 0,
                   "the unlocked tooltip recommends the plate-fixed datum")
            verify(toolTip.text.indexOf("reference frame") >= 0,
                   "the unlocked tooltip explains what a datum is")

            // The unlocked combo hovers through the same wrapper the locked one
            // does, so the explanation reaches the user either way.
            waitForRendering(rootId)
            mouseMove(datumCombo, datumCombo.width / 2, datumCombo.height / 2)
            tryVerify(() => toolTip.visible, 5000,
                      "hovering the unlocked datum combo should explain the datum")
        }

        //! Out at sea no plate-fixed frame reaches, so the tooltip explains the
        //! datum and stops — there is nothing to recommend.
        function test_theDatumTooltipRecommendsNothingWhereNoFrameReaches() {
            const cave = gotoFixStations()
            const model = cave.fixStations
            compare(model.addFixStation("A1"), 0)
            compare(model.setCoordinateText(0, "5.0, -150.0, 0m", Units.Metric), "")

            const datumCombo = datumComboFor(0)
            tryVerify(() => datumCombo.enabled, 5000, "a mid-ocean fix is still a fix")
            tryVerify(() => datumCombo.model.length === 1, 5000, "WGS84 alone")

            const toolTip = datumToolTipFor(0)
            verify(toolTip.text.indexOf("reference frame") >= 0,
                   "the tooltip still explains what a datum is")
            verify(toolTip.text.indexOf("holds better") < 0,
                   "there is no plate-fixed frame here to recommend")
        }

        //! The list carries the row's own datum whether the bounds check chose it
        //! or not, so a row that names one gets no recommendation: out at sea an
        //! ETRS89 row must not be told ETRS89 is the frame for where it sits.
        function test_theDatumTooltipRecommendsNothingForARowOnItsOwnDatum() {
            const cave = gotoFixStations()
            const model = cave.fixStations
            compare(model.addFixStation("A1"), 0)
            model.setData(model.index(0), "EPSG:4258", FixStationModel.InputCSRole)
            compare(model.setCoordinateText(0, "5.0, -150.0, 0m", Units.Metric), "")

            const datumCombo = datumComboFor(0)
            tryVerify(() => datumCombo.enabled, 5000, "a mid-ocean fix is still a fix")
            tryCompare(datumCombo, "displayText", "ETRS89")
            tryVerify(() => datumCombo.model.length === 2, 5000,
                      "WGS84 plus the datum the row stores")

            const toolTip = datumToolTipFor(0)
            verify(toolTip.text.indexOf("reference frame") >= 0,
                   "the tooltip still explains what a datum is")
            verify(toolTip.text.indexOf("holds better") < 0,
                   "the row's own datum is no recommendation")
        }

        //! Changing the datum reinterprets the numbers; it never moves them. And
        //! moving the coordinate never changes the datum — the warning beside the
        //! row is what reports a pairing that no longer fits.
        function test_aCoordinateEditNeverRewritesTheDatum() {
            const cave = gotoFixStations()
            const model = cave.fixStations
            compare(model.addFixStation("A1"), 0)
            // Slovenia, where ETRS89 is the plate-fixed frame.
            compare(model.setCoordinateText(0, "46.0, 14.5, 300m", Units.Metric), "")

            const datumCombo = datumComboFor(0)
            tryVerify(() => datumNamesOf(datumCombo).indexOf("ETRS89") >= 0, 5000,
                      "Europe's frame should be offered")

            const eastingBefore = model.data(model.index(0), FixStationModel.EastingRole)
            const northingBefore = model.data(model.index(0), FixStationModel.NorthingRole)

            datumCombo.activated(datumRowFor(datumCombo, "ETRS89"))
            tryVerify(() => model.data(model.index(0), FixStationModel.InputCSRole)
                            === "EPSG:4258", 5000,
                      "picking ETRS89 commits the ETRS89 geographic code")
            compare(model.data(model.index(0), FixStationModel.EastingRole), eastingBefore)
            compare(model.data(model.index(0), FixStationModel.NorthingRole), northingBefore)

            // The same fix, retyped in Tennessee: the list follows the numbers,
            // the choice does not.
            compare(model.setCoordinateText(0, "36.1, -85.5, 300m", Units.Metric), "")
            tryVerify(() => datumNamesOf(datumCombo).indexOf("NAD83(2011)") >= 0, 5000,
                      "North America's frame joins the list")
            compare(model.data(model.index(0), FixStationModel.InputCSRole), "EPSG:4258")
            tryCompare(datumCombo, "displayText", "ETRS89")
            verify(datumNamesOf(datumCombo).indexOf("ETRS89") >= 0,
                   "the combo can still display what the row is on")

            const warning = findChild(RootData.pageView.currentPageItem, "coordinateWarning.0")
            verify(warning !== null, "the row's coordinate warning should be reachable")
            tryVerify(() => warning.visible, 5000,
                      "an out-of-bounds pairing is what the warning reports")
        }

        //! UTM narrows the list twice over: to the datums whose series reaches
        //! the zone on screen, and to the ones the coordinate's region allows.
        function test_theUtmDatumListMeetsTheRegionList() {
            const cave = gotoFixStations()
            const model = cave.fixStations
            compare(model.addFixStation("A1"), 0)
            // NAD83(2011) UTM zone 16N, in Tennessee.
            model.setData(model.index(0), "EPSG:6345", FixStationModel.InputCSRole)
            compare(model.setCoordinateText(0, "610016.79, 3995117.07, 300m", Units.Metric), "")

            const datumCombo = datumComboFor(0)
            tryVerify(() => datumCombo.enabled, 5000, "the coordinate reads, so the datum opens")
            tryVerify(() => datumCombo.model.length === 2, 5000,
                      "WGS84 and NAD83(2011) both run a zone 16N series")
            compare(datumNamesOf(datumCombo), ["WGS84", "NAD83(2011)"])

            // ETRS89 reaches zone 32, but this coordinate is nowhere near Europe,
            // so the region list keeps it out.
            const zoneSpin = findChild(waitForPicker(0), "csUtmZone")
            zoneSpin.value = 32
            zoneSpin.valueModified()
            tryVerify(() => datumNamesOf(datumCombo).indexOf("ETRS89") < 0, 5000,
                      "a datum the region rules out stays out whatever the zone is")
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

            // A transposed easting, and then the system taken away. The numbers
            // stay in the row's text, but with no axis order to read them under
            // there is no point to place inside or outside a domain — so the row
            // reports nothing rather than guessing. Clearing the CS last is what
            // keeps the coordinate: writing a component without one discards it.
            model.setData(idx, 1478000.0, FixStationModel.EastingRole)
            model.setData(idx, "", FixStationModel.InputCSRole)
            verify(model.data(idx, FixStationModel.CoordinateTextRole).length > 0,
                   "the coordinate text must survive losing the CS")
            compare(diagnostics.data(proxyIdx, FixStationDiagnosticsModel.DomainErrorRole), "",
                    "a row that declares no CS is not judged at all")

            // The premise: that same coordinate does flag once the row says what
            // system it is in, so the check above can't pass for free.
            model.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            tryVerify(() => diagnostics.data(proxyIdx,
                                             FixStationDiagnosticsModel.DomainErrorRole) !== "",
                      5000, "naming the CS flags the transposed easting")
        }

        function test_coordinateWarningIconShowsInline() {
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
                warning = findChild(fixPage, "coordinateWarning.0")
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
            addProjectedFixStation(cave)
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const idx = model.index(0)
            model.setData(idx, 46.12113, FixStationModel.EastingRole)
            model.setData(idx, -115.59902, FixStationModel.NorthingRole)
            model.setData(idx, 304.0, FixStationModel.ElevationRole)

            const fixPage = RootData.pageView.currentPageItem
            let coordinateCell = null
            tryVerify(() => {
                coordinateCell = findChild(fixPage, "coordinateCell.0")
                return coordinateCell !== null
            }, 5000, "row 0 coordinate cell should be reachable")

            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 304.000m", 5000,
                       "the cell spells out the whole coordinate with its elevation unit, "
                       + "the elevation to the millimeter meters read to")

            // A component edited elsewhere has to move the field with it.
            model.setData(idx, 305.0, FixStationModel.ElevationRole)
            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 305.000m", 5000,
                       "the cell tracks the model rather than caching what it drew")

            // Switching the project to imperial has to move both the number and
            // the suffix: a bare elevation is read back in the project's units,
            // so a field that kept saying "305" would now mean 305 ft.
            // The elevation reads to the unit's own precision either way — a
            // hundredth of a foot here, a millimeter in meters — not to whatever
            // it takes to round-trip the double.
            const previousUnits = RootData.region.unitSystem
            RootData.region.unitSystem = Units.Imperial
            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 1000.66ft",
                       5000, "the elevation follows the project's unit system, suffix and all")

            RootData.region.unitSystem = previousUnits
            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 305.000m", 5000)
        }

        function test_coordinateCellCommitsAWholeCoordinate() {
            // The paste path, end to end through the page: one string in, three
            // model roles out, with the ft suffix converted on the way.
            const cave = gotoFixStations()
            addProjectedFixStation(cave)
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

            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 304.000m", 5000,
                       "a geographic row leads with the latitude")

            // Switching to a projected CS re-reads that same string easting
            // first, so the text on screen stays put and the components behind
            // it swap. That is the whole point of the coordinate being the
            // string: correcting the system corrects how it is read.
            model.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 304.000m", 5000,
                       "a projected row leads with the easting")
            compare(model.data(idx, FixStationModel.EastingRole), 46.12113,
                    "and the easting is now what the string leads with")
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
            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 304.000m", 5000)
            fixPage.commitCoordinate(0, coordinateCell.text)
            fuzzyCompare(model.data(idx, FixStationModel.NorthingRole), 46.12113, 1e-9)
            fuzzyCompare(model.data(idx, FixStationModel.EastingRole), -115.59902, 1e-9)
        }

        function test_coordinateEditorOffersTheStringTheUserTyped() {
            // U14. The two halves are deliberately different: the column renders
            // every row in the project's units so it can be scanned, while an
            // edit starts from what was written. Being handed a machine's
            // rounding of your own number and told to correct it is the
            // complaint this exists to answer.
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const fixPage = RootData.pageView.currentPageItem
            const coordinateCell = findCoordinateCell(0)

            fixPage.commitCoordinate(0, "46.12113, -115.59902, 30ft")

            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 9.144m", 5000,
                       "the cell renders the row in the project's units")
            tryCompare(coordinateCell, "editText", "46.12113, -115.59902, 30ft", 5000,
                       "the editor re-offers what was typed, feet and all")

            // And through the real path, since editText is only a property until
            // openEditor() puts it in the shadow input.
            coordinateCell.openEditor()
            compare(GlobalShadowTextInput.textInput.text, "46.12113, -115.59902, 30ft",
                    "the editor opens on the user's own string")
            coordinateCell.closeEditor()
        }

        function test_coordinateEditorFallsBackToTheCellForARowNobodyTyped() {
            // Which is most rows: a fix imported from svx/Compass/Walls, one
            // loaded from a project written before the string was kept, or one
            // whose numbers were set from anywhere but this field.
            const cave = gotoFixStations()
            addProjectedFixStation(cave)
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const idx = model.index(0)
            model.setData(idx, 46.12113, FixStationModel.EastingRole)
            model.setData(idx, -115.59902, FixStationModel.NorthingRole)
            model.setData(idx, 304.0, FixStationModel.ElevationRole)

            const fixPage = RootData.pageView.currentPageItem
            const coordinateCell = findCoordinateCell(0)

            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 304.000m", 5000)
            tryCompare(coordinateCell, "editText", "46.12113, -115.59902, 304.000m", 5000,
                       "with no string of its own, the editor opens on the cell")

            // A component written elsewhere invalidates a string that was there,
            // so the editor drops back to the cell rather than offering one that
            // no longer describes the row.
            fixPage.commitCoordinate(0, "46.12113, -115.59902, 30ft")
            tryCompare(coordinateCell, "editText", "46.12113, -115.59902, 30ft", 5000)

            model.setData(idx, 500.0, FixStationModel.ElevationRole)
            tryCompare(coordinateCell, "editText", "46.12113, -115.59902, 500.000m", 5000,
                       "the string went with the number it described")
        }

        function test_coordinateCellOpenedAndLeftWritesNothing() {
            // The commonest gesture on this page, and the one the no-op rule
            // exists for. In an imperial project the cell reads out to the
            // hundredth of a foot, so a commit that trusted what it shows would
            // move the fix by a quarter of an inch, dirty the project and
            // re-solve the plot. The editor opens on the stored string instead.
            const cave = gotoFixStations()
            addProjectedFixStation(cave)
            tryCompare(cave.fixStations, "count", 1)

            const previousUnits = RootData.region.unitSystem
            RootData.region.unitSystem = Units.Imperial

            const model = cave.fixStations
            const idx = model.index(0)
            model.setData(idx, 46.12113, FixStationModel.EastingRole)
            model.setData(idx, -115.59902, FixStationModel.NorthingRole)
            model.setData(idx, 1.0, FixStationModel.ElevationRole)

            const coordinateCell = findCoordinateCell(0)
            tryCompare(coordinateCell, "text", "46.12113, -115.59902, 3.28ft", 5000,
                       "an imperial project reads the elevation to the hundredth of a foot")

            // Watched, not just compared afterwards: a commit that rewrote the
            // row with the identical string would leave every value below
            // unchanged and still dirty the project and re-solve the plot.
            dataChangedSpyId.target = model
            dataChangedSpyId.clear()

            coordinateCell.openEditor()
            compare(coordinateCell.commitChanges(), true, "an untouched field commits")

            compare(dataChangedSpyId.count, 0, "leaving an untouched field writes nothing")
            compare(model.data(idx, FixStationModel.ElevationRole), 1.0,
                    "and the elevation is exactly where it was, to the last bit")
            compare(model.data(idx, FixStationModel.CoordinateTextRole),
                    "46.12113, -115.59902, 1.000m",
                    "the stored coordinate is untouched — the editor opened on it, "
                    + "not on the imperial rendering the cell shows")

            RootData.region.unitSystem = previousUnits
        }

        function test_coordinateValidatorHoldsBackTextItCantRead() {
            // #621 asks for a validator. It must never report Invalid — that
            // rejects the keystroke and no coordinate is complete while it's
            // being typed — so partial text is Intermediate, which
            // CoreClickTextInput refuses to commit while showing the reason.
            const cave = gotoFixStations()
            addProjectedFixStation(cave)
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

            // The row is on a projected system, so the field reads easting first.
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
            addProjectedFixStation(cave)
            tryCompare(cave.fixStations, "count", 1)

            const model = cave.fixStations
            const idx = model.index(0)

            const fixPage = RootData.pageView.currentPageItem
            let coordinateCell = null
            tryVerify(() => {
                coordinateCell = findChild(fixPage, "coordinateCell.0")
                return coordinateCell !== null
            }, 5000, "row 0 coordinate cell should be reachable")

            // A projected row leads with the easting.
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
            addProjectedFixStation(cave)
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


        //! A row carrying a coordinate with no system to read it under. The
        //! ordinary shape of one — an svx `*fix` with no `*cs`, or any project
        //! saved before fixes carried their own system — and reachable from here
        //! because such a row's text still parses; it is only meaningless.
        function makeSystemlessRow(cave, coordinate) {
            const model = cave.fixStations
            model.addFixStation()
            const row = model.count - 1
            model.setData(model.index(row), "", FixStationModel.InputCSRole)
            compare(model.setCoordinateText(row, coordinate, ProjectUnits.unitSystem), "",
                    "the text itself has to be readable, or the row would be Unreadable")
            return row
        }

        function test_aCoordinateWithNoSystemShowsItsTextAndSaysWhatIsMissing() {
            // Until this, such a row rendered as "0, 0, 0m" with no complaint —
            // indistinguishable from a fix genuinely entered at the origin, and
            // with the text that is actually on it visible only to whoever
            // thought to open the editor.
            const cave = gotoFixStations()
            // Space-separated on purpose: the row's own text and the rendering
            // of the numbers read out of it are then different strings, so the
            // two assertions below can tell each other apart. Comma-separated
            // text renders back identically and would let the cell go on showing
            // raw text after the system is named without anything failing.
            const row = makeSystemlessRow(cave, "610016.792 5615117.075 304m")

            const cell = findCoordinateCell(row)
            // The cell shows the text, as written, since there are no numbers.
            tryCompare(cell, "value", "610016.792 5615117.075 304m", 5000)
            verify(cell.error, "and flags it, because the row anchors nothing")
            compare(cell.color, Theme.errorText)

            const warning = findChild(RootData.pageView.currentPageItem, "coordinateWarning." + row)
            verify(warning !== null, "row " + row + " warning should be reachable")
            tryVerify(() => warning.visible, 5000, "the warning shows for a row with no system")
            verify(warning.message.indexOf("coordinate system") >= 0,
                   "and asks for the system rather than blaming the text: " + warning.message)

            // Naming the system it was always in is all it takes: the same text,
            // now read, renders as numbers and the complaint goes away.
            waitForPicker(row).committed("EPSG:32611")
            tryVerify(() => !cell.error, 5000, "naming the system clears the flag")
            compare(cell.value, "610016.792, 5615117.075, 304.000m",
                    "and the cell now renders the numbers it read out of that text, "
                    + "rather than going on showing the text")
            tryVerify(() => !warning.visible, 5000, "and the warning goes with it")
        }

        function test_aCoordinateThatCantBeReadShowsTheTextAndTheReason() {
            // The hand-edited file, which this surface has no way to write —
            // setCoordinateText() refuses anything the parser won't take. A
            // non-finite component is the one door left: the fix spells it out
            // as "inf" and then reads its own output back, so the row is
            // honestly Unreadable rather than Valid with a string that disagrees
            // with it (see cwFixStation::write).
            const cave = gotoFixStations()
            const row = addProjectedFixStation(cave)
            const model = cave.fixStations
            model.setData(model.index(row), Infinity, FixStationModel.EastingRole)

            const stored = model.data(model.index(row), FixStationModel.CoordinateTextRole)
            verify(stored.indexOf("inf") >= 0, "the row should be carrying unreadable text")

            const cell = findCoordinateCell(row)
            // The offending text is shown, not the zeros the row reports.
            tryCompare(cell, "value", stored, 5000)
            verify(cell.error, "and it is flagged")
            compare(cell.color, Theme.errorText)

            const warning = findChild(RootData.pageView.currentPageItem, "coordinateWarning." + row)
            tryVerify(() => warning.visible, 5000, "the warning shows for unreadable text")
            verify(warning.message.indexOf("can't be read") >= 0,
                   "and blames the text: " + warning.message)
            verify(warning.message.indexOf("coordinate system") < 0,
                   "the row has a system — asking for one would send the user nowhere")
        }

        function test_namingAGeographicCSOnASystemlessRowAsksWhichWayRound() {
            // The one thing nothing can recover afterwards. The text was written
            // easting first and no one wrote that down, so reading it latitude
            // first transposes the fix — and both readings are numbers that look
            // like a coordinate, so nothing downstream can tell.
            const cave = gotoFixStations()
            const row = makeSystemlessRow(cave, "46.12113, -115.59902, 304m")
            const model = cave.fixStations

            const askBox = findChild(RootData.pageView.currentPageItem, "coordinateOrderAskBox")
            verify(askBox !== null, "the page should host the order question")
            verify(!askBox.opened, "nothing has been asked yet")

            waitForPicker(row).committed("EPSG:4326")
            tryVerify(() => askBox.opened, 5000,
                      "naming a geographic system on such a row has to ask")

            const asWritten = findChild(askBox, "coordinateOrderAskAsWritten")
            const swapped = findChild(askBox, "coordinateOrderAskSwapped")
            compare(asWritten.text, "46.12113, -115.59902, 304m",
                    "the reading it just committed to")
            compare(swapped.text, "-115.59902, 46.12113, 304m",
                    "and the other one, as the user's own text")

            findChild(askBox, "coordinateOrderSwapButton").clicked()
            tryVerify(() => !askBox.opened, 5000, "answering closes the question")
            tryCompare(model, "count", 1)
            compare(model.data(model.index(row), FixStationModel.CoordinateTextRole),
                    "-115.59902, 46.12113, 304m", "the swap is written to the row")
            // Which is the whole point: the numbers now mean what the user says
            // they mean.
            compare(model.data(model.index(row), FixStationModel.NorthingRole), -115.59902)
            compare(model.data(model.index(row), FixStationModel.EastingRole), 46.12113)
        }

        function test_keepingTheOrderAsWrittenChangesNothing() {
            const cave = gotoFixStations()
            const row = makeSystemlessRow(cave, "46.12113, -115.59902, 304m")
            const model = cave.fixStations

            const askBox = findChild(RootData.pageView.currentPageItem, "coordinateOrderAskBox")
            waitForPicker(row).committed("EPSG:4326")
            tryVerify(() => askBox.opened, 5000)

            findChild(askBox, "coordinateOrderKeepButton").clicked()
            tryVerify(() => !askBox.opened, 5000)
            compare(model.data(model.index(row), FixStationModel.CoordinateTextRole),
                    "46.12113, -115.59902, 304m",
                    "the text stays exactly as it was — the reading was already committed")
        }

        function test_theOrderQuestionIsOnlyAskedWhenItCanBeAnswered() {
            const cave = gotoFixStations()
            const askBox = findChild(RootData.pageView.currentPageItem, "coordinateOrderAskBox")

            // A projected system reads the text in the order it was written, so
            // there is nothing at stake and nothing to ask.
            const projected = makeSystemlessRow(cave, "610016.792, 5615117.075, 304m")
            waitForPicker(projected).committed("EPSG:32611")
            wait(50)
            verify(!askBox.opened, "a projected system reads the text straight back")

            // Nor is a row that already had a system: whatever order its text is
            // in, that order is recorded, so changing systems re-reads rather
            // than guesses.
            const model = cave.fixStations
            waitForPicker(projected).committed("EPSG:4326")
            wait(50)
            verify(!askBox.opened, "a row with a system has an order already")

            // And a row nobody typed into has no text to be unsure about.
            model.addFixStation()
            const blank = model.count - 1
            model.setData(model.index(blank), "", FixStationModel.InputCSRole)
            waitForPicker(blank).committed("EPSG:4326")
            wait(50)
            verify(!askBox.opened, "an empty row has nothing to transpose")

            // The positive control, in this same test: the box does open when
            // the question can be answered. Without it the three silences above
            // would also be satisfied by a dialog that never opens at all, or by
            // 50 ms that was never long enough to see one.
            const systemless = makeSystemlessRow(cave, "610016.792, 5615117.075, 304m")
            waitForPicker(systemless).committed("EPSG:4326")
            tryVerify(() => askBox.opened, 5000,
                      "a system-less row about to be read latitude-first is asked about")
            askBox.close()
        }

        function test_theNarrowLayoutAsksWhichWayRoundAsWell() {
            // The narrow layout is a separate delegate with its own
            // coordinate-system cell and its own call into commitCS(), and
            // nothing else in this file exercises it. A transposition it failed
            // to ask about would be exactly as unrecoverable as one the wide
            // layout missed.
            const cave = gotoFixStations()
            const page = RootData.pageView.currentPageItem
            const askBox = findChild(page, "coordinateOrderAskBox")
            const row = makeSystemlessRow(cave, "610016.792, 5615117.075, 304m")

            rootId.width = Theme.breakpointPanelCollapse - 100
            tryVerify(() => page.isNarrow, 5000, "the page should be in its narrow layout")

            waitForPicker(row).committed("EPSG:4326")
            tryVerify(() => askBox.opened, 5000,
                      "the narrow layout asks which way round too")
            compare(askBox.row, row, "and asks about the row whose cell was committed")

            // And the answer gets back to the model from here, not just onto the
            // dialog — the narrow delegate's warning and cell read it back.
            const swapButton = findChild(askBox, "coordinateOrderSwapButton")
            verify(swapButton !== null, "the swap button should be reachable")
            mouseClick(swapButton)

            const model = cave.fixStations
            tryCompare(model, "count", row + 1, 5000)
            tryVerify(() => model.data(model.index(row), FixStationModel.CoordinateTextRole)
                            === "5615117.075, 610016.792, 304m",
                      5000, "the swap reaches the row from the narrow layout")

            rootId.width = defaultWindowWidth
            tryVerify(() => !page.isNarrow, 5000, "and the layout goes back")
        }

        function test_aQuestionThatCantBeAskedLeavesTheOpenOneAlone() {
            // askAbout() decides whether to ask, and a call that decides not to
            // must leave without touching anything — otherwise it repoints a
            // question already on screen at a row it declined to ask about, and
            // the answer lands on the wrong fix.
            //
            // Driven through askAbout() directly: the dialog is modal, so no
            // sequence of clicks can currently reach a second commit while it is
            // up. That modality is what makes this unreachable today, and the
            // ordering is what keeps it from being the only thing holding it up.
            const cave = gotoFixStations()
            const askBox = findChild(RootData.pageView.currentPageItem, "coordinateOrderAskBox")

            const asked = makeSystemlessRow(cave, "610016.792, 5615117.075, 304m")
            waitForPicker(asked).committed("EPSG:4326")
            tryVerify(() => askBox.opened, 5000, "the question is up")
            compare(askBox.row, asked)

            // Text no arrangement can read, so there is nothing to offer and
            // this call bails — after the first guard, which it passes.
            askBox.askAbout(asked + 1, "N 46 07 16 W 115 35 56", "EPSG:4326", true)

            compare(askBox.row, asked,
                    "the open question still points at the row it was asked about")
            compare(askBox.coordinate, "610016.792, 5615117.075, 304m",
                    "and still shows that row's text")
            verify(askBox.opened, "and is still up")
            askBox.close()
        }

        //! The page swaps layouts on whether the wide table fits, so the two
        //! sides of that answer are checked a couple of pixels apart. The wide
        //! side also proves the point of the whole exercise: the pick button,
        //! the rightmost thing a row draws, is still on screen.
        function test_theLayoutFollowsWhetherTheWideTableFits() {
            const cave = gotoFixStations()
            const page = RootData.pageView.currentPageItem
            cave.fixStations.addFixStation()

            // The window is wider than the page by whatever chrome sits beside
            // it, so the page is driven to a width, not the window.
            const chrome = rootId.width - page.width
            const fits = Math.ceil(page.wideMinimumWidth)

            rootId.width = fits + chrome - 2
            tryVerify(() => findChild(page, "narrowFixRow.0") !== null, 5000,
                      "a page too narrow for the table takes the narrow layout")

            rootId.width = fits + chrome + 2
            tryVerify(() => findChild(page, "narrowFixRow.0") === null, 5000,
                      "and it takes the wide table back once the table fits")

            let pickButton = null
            tryVerify(() => {
                pickButton = findChild(page, "pickFromViewButton.0")
                return pickButton !== null && pickButton.visible
            }, 5000, "the wide row's pick button should be on screen")

            const rightEdge = pickButton.mapToItem(page, pickButton.width, 0).x
            verify(rightEdge <= page.width,
                   "the pick button's right edge (" + rightEdge
                   + ") should sit inside the page (" + page.width + ")")
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
