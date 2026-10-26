import QtQuick
import QtQuick.Controls as QC
import QtQuick.Templates as T
import QtCore as QtCore
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    //! Hovers \a host through \a testCase and returns the window's shared
    //! tooltip once it shows on \a host.
    function hoverSharedToolTip(testCase, host) {
        testCase.waitForRendering(rootId)
        testCase.mouseMove(host, host.width / 2, host.height / 2)
        const shared = host.QC.ToolTip.toolTip
        testCase.tryVerify(() => shared.visible && shared.parent === host, 5000,
                           "hovering " + host.objectName + " shows the window's shared tooltip")
        return shared
    }

    //! Hovers \a host through \a testCase past the tooltip delay and checks
    //! that the window's shared tooltip stays off it. Waits a fixed time,
    //! since the point is that nothing shows.
    function verifyNoSharedToolTip(testCase, host) {
        testCase.waitForRendering(rootId)
        testCase.mouseMove(host, host.width / 2, host.height / 2)
        const shared = host.QC.ToolTip.toolTip
        testCase.wait(Theme.toolTipDelay * 2)
        testCase.verify(!(shared.visible && shared.parent === host),
                        "hovering " + host.objectName + " shows no tooltip; it reads \"" + shared.text + "\"")
        testCase.mouseMove(rootId, 0, 0)
    }

    //! Moves the pointer off the page through \a testCase and waits for \a tip to hide.
    function leaveToolTip(testCase, tip) {
        testCase.mouseMove(rootId, 0, 0)
        testCase.tryVerify(() => !tip.visible, 5000, "leaving hides the tooltip")
    }

    //! Switches to the light scheme, hovers \a host through \a testCase, and
    //! checks that its tooltip shows text holding \a text in the Theme's
    //! tooltip colors. The caller puts the scheme back.
    function verifyLightToolTip(testCase, host, text) {
        RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.Light
        testCase.tryVerify(() => !Theme.dark, 5000, "the scheme is light")

        const tip = rootId.hoverSharedToolTip(testCase, host)
        testCase.verify(tip.contentItem.text.indexOf(text) >= 0,
                        "the tooltip reads \"" + tip.contentItem.text + "\"")
        testCase.verify(Qt.colorEqual(tip.background.color, Theme.toolTipSurface),
                        "tooltip surface " + tip.background.color + ", Theme " + Theme.toolTipSurface)
        testCase.verify(Qt.colorEqual(tip.contentItem.color, Theme.toolTipText),
                        "tooltip text " + tip.contentItem.color + ", Theme " + Theme.toolTipText)
        rootId.leaveToolTip(testCase, tip)
    }

    //! The first item under \a item whose objectName starts with \a prefix.
    function findChildWithPrefix(item, prefix) {
        if (item === null) {
            return null
        }
        const children = item.children
        for (let i = 0; i < children.length; ++i) {
            const child = children[i]
            if (String(child.objectName).startsWith(prefix)) {
                return child
            }
            const found = rootId.findChildWithPrefix(child, prefix)
            if (found !== null) {
                return found
            }
        }
        return null
    }

    //! How many items under \a item, hidden ones included, satisfy \a matches.
    function countDescendants(item, matches) {
        let count = 0
        if (item === null) {
            return count
        }
        const children = item.children
        for (let i = 0; i < children.length; ++i) {
            const child = children[i]
            if (matches(child)) {
                ++count
            }
            count += rootId.countDescendants(child, matches)
        }
        return count
    }

    //! How many items under \a item, hidden ones included, have an objectName
    //! starting with \a prefix.
    function countWithPrefix(item, prefix) {
        return rootId.countDescendants(item, child => String(child.objectName).startsWith(prefix))
    }

    //! How many CSComboBox items sit under \a item, hidden ones included.
    function countCSComboBoxes(item) {
        return rootId.countDescendants(item, child => child instanceof CSComboBox)
    }

    //! The visible descendants of \a item whose text contains \a text.
    function itemsShowingText(item, text) {
        let found = []
        if (item === null) {
            return found
        }
        const children = item.children
        for (let i = 0; i < children.length; ++i) {
            const child = children[i]
            if (child.visible && typeof child.text === "string" && child.text.indexOf(text) >= 0) {
                found.push(child)
            }
            found = found.concat(rootId.itemsShowingText(child, text))
        }
        return found
    }

    //! Whether some item under \a item shows text containing \a text.
    function showsText(item, text) {
        return rootId.itemsShowingText(item, text).length > 0
    }

    //! The page's header cell titled \a title, found among the visual
    //! children by its sortMode, null when there is none.
    function headerCell(title) {
        const search = (item) => {
            if (item.sortMode !== undefined && item.text === title) {
                return item
            }
            for (let i = 0; i < item.children.length; ++i) {
                const found = search(item.children[i])
                if (found !== null) {
                    return found
                }
            }
            return null
        }
        return search(RootData.pageView.currentPageItem)
    }

    // Opened on the page's own category so cleanup() can put back the widths
    // a test dragged: one QSettings serves the whole run.
    QtCore.Settings {
        id: fixColumnSettingsId
        category: "FixStationPage"
    }

    ExternalCenterlineTestCase {
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
            rootId.closeAnyOpenEditor()

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
            RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.System
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()

            // Puts back the widths a test dragged, on the page's own category.
            for (const key of columnWidthKeys) {
                fixColumnSettingsId.setValue(key, 0)
            }
            fixColumnSettingsId.sync()
            RootData.settings.fontSettings.fontBaseSize = RootData.settings.fontSettings.defaultFontBaseSize
        }

        readonly property string coordinateTitle: "Coordinate (East, North / Lat, Long)"
        readonly property list<string> columnWidthKeys: ["stationColumnWidth", "inputCSColumnWidth", "coordinateColumnWidth"]
        readonly property int columnDrag: 80
        readonly property int columnWidthTolerance: 2
        readonly property int largerFontStep: 4

        //! The resize grip on the right edge of header \a title.
        function resizeGripOf(title) {
            const cell = rootId.headerCell(title)
            verify(cell !== null, "the page has a " + title + " header")
            const grip = rootId.findChildWithPrefix(cell, "columnResizeGrip")
            verify(grip !== null, "the " + title + " header has a resize grip")
            verify(grip.visible, "the " + title + " header's resize grip shows")
            return grip
        }

        //! Drags header \a title's right edge \a dx pixels sideways. The drag
        //! is laid out on the page because the grip follows the edge it moves,
        //! and mouseDrag maps each step through its item's current position.
        function dragColumnEdge(title, dx) {
            const grip = resizeGripOf(title)
            const page = RootData.pageView.currentPageItem
            const start = grip.mapToItem(page, grip.width / 2, grip.height / 2)
            mouseDrag(page, start.x, start.y, dx, 0)
        }

        function gotoFixStations() {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = "PR4Cave"

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Node=" + String(cave.name) + "/Fix Stations"
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
        //! has built it. At rest the row shows its system as text, and a
        //! double-click on that text opens the picker.
        function waitForPicker(rowIndex) {
            const fixPage = RootData.pageView.currentPageItem
            let picker = null
            let label = null
            tryVerify(() => {
                picker = findPicker(rowIndex)
                label = findChild(fixPage, "inputCSLabel." + rowIndex)
                return picker !== null || (label !== null && label.visible)
            }, 5000, "row " + rowIndex + " should show its coordinate system")
            if (picker === null) {
                doubleTap(label)
                tryVerify(() => {
                    picker = findPicker(rowIndex)
                    return picker !== null
                }, 5000, "double-clicking row " + rowIndex + "'s system should open its picker")
            }
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

            const tableView = findChild(fixPage, "fixStationTreeView")
            verify(tableView !== null, "fixStationTreeView must exist")
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

            const tableView = findChild(fixPage, "fixStationTreeView")
            tryCompare(tableView, "count", 1)

            const help = findChild(fixPage, "noFixStationsHelpBox")
            tryVerify(() => !help.visible, 2000, "help box should hide once a fix is added")
        }

        function test_aPlainOpenPicksNoRow() {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = "PR4Cave"
            cave.fixStations.addFixStation("A1")
            cave.fixStations.addFixStation("A2")

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Node=" + String(cave.name) + "/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should land on fixStationPage")

            const tableView = findChild(RootData.pageView.currentPageItem, "fixStationTreeView")
            verify(tableView !== null, "fixStationTreeView must exist")
            tryCompare(tableView, "count", 2)
            compare(tableView.currentIndex, -1)
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

            const picker = waitForPicker(0)

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

        //! Row \a rowIndex's at-rest system text, once the row shows it.
        function waitForCSLabel(rowIndex) {
            const fixPage = RootData.pageView.currentPageItem
            let label = null
            tryVerify(() => {
                label = findChild(fixPage, "inputCSLabel." + rowIndex)
                return label !== null && label.visible
            }, 5000, "row " + rowIndex + " shows its coordinate system as text")
            return label
        }

        //! Waits until row \a rowIndex's station cell exists, so the row is built.
        function waitForRow(rowIndex) {
            tryVerify(() => findChild(RootData.pageView.currentPageItem,
                                      "stationCell." + rowIndex) !== null,
                      5000, "row " + rowIndex + " is on the page")
        }

        // At rest an editable row names its system in plain text; the picker
        // loads only once a double-click on that text opens it.
        function test_anEditableRowShowsItsSystemAsTextUntilDoubleClicked() {
            const cave = gotoFixStations()
            const model = cave.fixStations
            model.addFixStation()
            model.setData(model.index(0), "EPSG:32612", FixStationModel.InputCSRole)
            waitForRow(0)

            compare(findPicker(0), null, "an editable row builds no picker at rest")

            const label = waitForCSLabel(0)
            compare(label.text, CSFormat.displayName("EPSG:32612"),
                    "the row names its system the way the picker does")
            verify(Qt.colorEqual(label.color, Theme.text),
                   "the system reads in the ordinary text color, " + label.color
                   + ", Theme " + Theme.text)

            const tip = rootId.hoverSharedToolTip(this, label)
            verify(tip.text.indexOf(CSFormat.displayName("EPSG:32612")) >= 0,
                   "the tooltip names the full system: \"" + tip.text + "\"")
            rootId.leaveToolTip(this, tip)

            const picker = waitForPicker(0)
            tryCompare(picker, "currentMode", CoordinateSystem.UTM)

            picker.committed("EPSG:4326")
            tryVerify(() => model.data(model.index(0), FixStationModel.InputCSRole) === "EPSG:4326",
                      5000, "the opened picker commits onto the row")
        }

        // A blank system reads "none"; a single click selects the row and
        // leaves the picker closed.
        function test_aBlankEditableSystemReadsNoneAndASingleClickSelectsTheRow() {
            const cave = gotoFixStations()
            const model = cave.fixStations
            model.addFixStation()
            model.setData(model.index(0), "", FixStationModel.InputCSRole)
            waitForRow(0)
            const page = RootData.pageView.currentPageItem
            const treeView = findChild(page, "fixStationTreeView")

            compare(findPicker(0), null, "a blank editable row builds no picker at rest")
            const label = waitForCSLabel(0)
            compare(label.text, "none")

            treeView.currentIndex = -1
            waitForRendering(rootId)
            mouseClick(label)
            tryCompare(treeView, "currentIndex", 0, 5000, "a single click selects the row")
            compare(findPicker(0), null, "and leaves the picker closed")
        }

        // Rows renumber when one is added, so the open picker closes rather
        // than land on another fix; Escape closes it too.
        function test_thePickerClosesOnEscapeAndWhenRowsShift() {
            const cave = gotoFixStations()
            const page = RootData.pageView.currentPageItem
            const model = cave.fixStations
            model.addFixStation()
            model.setData(model.index(0), "EPSG:32612", FixStationModel.InputCSRole)
            waitForRow(0)

            waitForPicker(0)
            compare(page.editingCSRow, 0)
            model.addFixStation()
            tryCompare(page, "editingCSRow", -1, 5000, "a row added closes the picker")
            tryVerify(() => findPicker(0) === null, 5000, "and unloads it")

            // A second double-tap this soon would count as taps three and four.
            page.editingCSRow = 0
            tryVerify(() => findPicker(0) !== null, 5000, "the picker reopens")
            keyClick(Qt.Key_Escape)
            tryCompare(page, "editingCSRow", -1, 5000, "Escape closes the picker")
            tryVerify(() => findPicker(0) === null, 5000, "and the row shows its text again")
        }

        // The narrow layout's row shares the at-rest text and opens the same
        // picker on a double-click.
        function test_theNarrowLayoutOpensThePickerOnADoubleClick() {
            const cave = gotoFixStations()
            const page = RootData.pageView.currentPageItem
            const model = cave.fixStations
            model.addFixStation()
            model.setData(model.index(0), "EPSG:32612", FixStationModel.InputCSRole)

            rootId.width = Theme.breakpointPanelCollapse - 100
            tryVerify(() => page.isNarrow, 5000, "the page should be in its narrow layout")
            tryVerify(() => findChild(page, "narrowFixRow.0") !== null, 5000,
                      "the row takes its narrow delegate")

            compare(findPicker(0), null, "the narrow row builds no picker at rest")
            const label = waitForCSLabel(0)
            compare(label.text, CSFormat.displayName("EPSG:32612"))

            waitForRendering(rootId)
            const picker = waitForPicker(0)
            picker.committed("EPSG:4326")
            tryVerify(() => model.data(model.index(0), FixStationModel.InputCSRole) === "EPSG:4326",
                      5000, "the narrow picker commits onto the row")

            rootId.width = defaultWindowWidth
            tryVerify(() => !page.isNarrow, 5000, "and the layout goes back")
        }

        function test_inputCSPickerModes() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const picker = waitForPicker(0)

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

            const picker = waitForPicker(0)

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
            // openEditor() puts it in the shadow editor.
            coordinateCell.openEditor()
            compare(rootId.shadowEditor.editorText(), "46.12113, -115.59902, 30ft",
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
            rootId.shadowEditor.setEditorText("46.12113, -115.59902, 304 bananas")
            compare(coordinateCell.commitChanges(), false, "the commit is refused")
            verify(coordinateCell.isEditting, "and the editor stays open on the bad text")
            verify(rootId.shadowEditor.hasError(), "with the reason shown where the user is typing")
            compare(rootId.shadowEditor.currentEditor.errorHelpBox.text, validator.errorText,
                    "and it is the validator's reason")
            compare(model.data(idx, FixStationModel.EastingRole), 0,
                    "nothing reached the model")

            // The row is on a projected system, so the field reads easting first.
            rootId.shadowEditor.setEditorText("610016.792, 5615117.075, 304m")
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

        // #727: a named fix on a cave with nothing to solve is flagged as an
        // unknown station right away, and the flag clears once a trip surveys
        // that station and the solve lands.
        function test_stationWarningShowsForUnknownStationOnBareCave() {
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
            tryVerify(() => warning.visible, 5000,
                      "a named fix on a bare cave shows the station-reference warning")
            verify(warning.message.indexOf("No survey station named \"A1\"") >= 0,
                   "the warning names the unknown station, got: " + warning.message)

            addTripNamingStations(cave, ["A1", "A2"])
            tryVerify(() => !warning.visible, 5000,
                      "surveying A1 clears the unknown-station warning")
        }

        function test_stationWarningShownForEmptyName() {
            // A blank row names no survey station — survex drops such a fix — so
            // it is flagged inline the moment it appears.
            const cave = gotoFixStations()
            addTripNamingStations(cave, ["A1", "A2"])
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
            tryVerify(() => warning.visible, 5000,
                      "empty station name → station-reference warning shows")

            // Typing a name clears it: A1 is a surveyed station, so the name
            // resolves. The flag is specific to the name being missing.
            model.setData(model.index(0), "A1", FixStationModel.StationNameRole)
            tryVerify(() => !warning.visible, 5000,
                      "naming a surveyed station clears the warning")
        }


        // The warning's tooltip is a dark panel in both schemes, so its text
        // takes the tooltip color, not the page's.
        function test_inlineWarningToolTipFollowsTheLightTheme() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            tryCompare(cave.fixStations, "count", 1)

            const fixPage = RootData.pageView.currentPageItem
            let warning = null
            tryVerify(() => {
                warning = findChild(fixPage, "stationWarning.0")
                return warning !== null && warning.visible
            }, 5000, "an empty station name shows the row's warning")
            rootId.verifyLightToolTip(this, warning, warning.message)
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

        // Dragging a header's right edge narrows that column, the fix rows'
        // cells follow it, and the width comes back when the page reopens.
        function test_aDraggedColumnEdgeResizesAndPersists() {
            const cave = gotoFixStations()
            const page = RootData.pageView.currentPageItem
            cave.fixStations.addFixStation()
            const coordinateCell = findCoordinateCell(0)
            waitForRendering(page)

            const header = rootId.headerCell(coordinateTitle)
            verify(header !== null, "the page has a Coordinate header")
            const startWidth = header.width
            const startMinimum = page.wideMinimumWidth

            dragColumnEdge(coordinateTitle, -columnDrag)
            tryVerify(() => Math.abs(header.width - (startWidth - columnDrag)) <= columnWidthTolerance, 5000,
                      "the Coordinate header narrows by the drag, from " + startWidth
                      + " to about " + (startWidth - columnDrag) + ", got " + header.width)
            const draggedWidth = header.width
            tryCompare(coordinateCell.parent, "width", draggedWidth, 5000,
                       "the fix row's coordinate cell follows its column")
            verify(Math.abs(page.wideMinimumWidth - (startMinimum - columnDrag)) <= columnWidthTolerance,
                   "the table needs less room once a column narrows")
            verify(!page.isNarrow, "the page keeps the wide layout")

            compare(Number(fixColumnSettingsId.value("coordinateColumnWidth", 0)), draggedWidth,
                    "the dragged width is stored under the page's settings")

            // A project switch evicts the cached page, so the next one is
            // built fresh and reads its widths back from the settings.
            RootData.newProject()
            const reopenedCave = gotoFixStations()
            const reopenedPage = RootData.pageView.currentPageItem
            verify(reopenedPage !== page, "the reopened page is a new page item")
            reopenedCave.fixStations.addFixStation()
            let reopenedHeader = null
            tryVerify(() => {
                reopenedHeader = rootId.headerCell(coordinateTitle)
                return reopenedHeader !== null
            }, 5000, "the reopened page has a Coordinate header")
            tryCompare(reopenedHeader, "width", draggedWidth, 5000,
                       "the dragged width survives reopening the page")
            tryCompare(findCoordinateCell(0).parent, "width", draggedWidth, 5000,
                       "and the reopened row's cell has it too")
        }

        // A drag far past the left edge stops at the minimum column width.
        function test_aColumnNarrowsNoFurtherThanItsMinimum() {
            gotoFixStations()
            const page = RootData.pageView.currentPageItem
            waitForRendering(page)
            verify(Theme.tableColumnMinimumWidth > 0, "Theme names a minimum column width")

            dragColumnEdge("Input CS", -1000)
            tryCompare(rootId.headerCell("Input CS"), "width", Theme.tableColumnMinimumWidth, 5000,
                       "the Input CS column stops at the minimum width")
        }

        // Widening a column stops where the table still fits the page, so the
        // page keeps the wide layout through the drag.
        function test_aColumnWidensOnlyAsFarAsThePageHolds() {
            const cave = gotoFixStations()
            const page = RootData.pageView.currentPageItem
            cave.fixStations.addFixStation()
            findCoordinateCell(0)
            waitForRendering(page)

            const header = rootId.headerCell("Input CS")
            verify(header !== null, "the page has an Input CS header")
            const startWidth = header.width
            const room = Math.floor(page.width - page.wideMinimumWidth)
            verify(room > 0, "the page has room for the column to widen")

            dragColumnEdge("Input CS", 5000)
            tryVerify(() => Math.abs(header.width - (startWidth + room)) <= 2, 5000,
                      "the Input CS column widens from " + startWidth + " to the page's spare room, about "
                      + (startWidth + room) + ", got " + header.width)
            verify(!page.isNarrow, "the page keeps the wide layout")
            verify(findChild(page, "narrowFixRow.0") === null, "the row keeps its wide delegate")
            verify(page.wideMinimumWidth <= page.width,
                   "the widened table (" + page.wideMinimumWidth + ") still fits the page ("
                   + page.width + ")")
        }

        //! Right-clicks header \a title and returns the header's menu area
        //! once its menu is open.
        function openColumnHeaderMenu(title) {
            const page = RootData.pageView.currentPageItem
            const menuArea = findChild(page, "fixColumnHeaderMenu")
            verify(menuArea !== null, "the column header carries a context menu")
            const cell = rootId.headerCell(title)
            verify(cell !== null, "the page has a " + title + " header")
            mouseClick(cell, cell.width / 2, cell.height / 2, Qt.RightButton)
            tryVerify(() => menuArea.menu.opened, 5000,
                      "right-clicking the " + title + " header opens its menu")
            return menuArea
        }

        //! The item named \a name in \a menuArea's menu, null when there is none.
        function menuItemNamed(menuArea, name) {
            const menu = menuArea.menu
            for (let i = 0; i < menu.count; ++i) {
                const item = menu.itemAt(i)
                if (item !== null && item.objectName === name) {
                    return item
                }
            }
            return null
        }

        // The header's "Reset column widths" puts a dragged column back at its
        // default width and stores the default sentinel in its place.
        function test_resetColumnWidthsRestoresTheDefaults() {
            const cave = gotoFixStations()
            const page = RootData.pageView.currentPageItem
            cave.fixStations.addFixStation()
            findCoordinateCell(0)
            waitForRendering(page)

            const header = rootId.headerCell(coordinateTitle)
            verify(header !== null, "the page has a Coordinate header")
            const defaultWidth = header.width

            let menuArea = openColumnHeaderMenu(coordinateTitle)
            let resetItem = menuItemNamed(menuArea, "resetColumnWidthsMenuItem")
            verify(resetItem !== null, "the header menu offers Reset column widths")
            verify(!resetItem.enabled, "the reset is disabled while every column has its default width")
            menuArea.menu.close()
            tryVerify(() => !menuArea.menu.visible, 5000, "the header menu closes")

            dragColumnEdge(coordinateTitle, -columnDrag)
            tryVerify(() => Math.abs(header.width - (defaultWidth - columnDrag)) <= columnWidthTolerance, 5000,
                      "the Coordinate header narrows by the drag, got " + header.width)
            const draggedWidth = header.width
            compare(Number(fixColumnSettingsId.value("coordinateColumnWidth", 0)), draggedWidth,
                    "the dragged width is stored")

            menuArea = openColumnHeaderMenu(coordinateTitle)
            resetItem = menuItemNamed(menuArea, "resetColumnWidthsMenuItem")
            verify(resetItem !== null, "the header menu offers Reset column widths")
            verify(resetItem.enabled, "the reset is enabled once a column has a stored width")
            mouseClick(resetItem)
            tryVerify(() => !menuArea.menu.visible, 5000, "choosing the reset closes the menu")

            tryCompare(header, "width", defaultWidth, 5000,
                       "the Coordinate header returns to its default width")
            tryCompare(findCoordinateCell(0).parent, "width", defaultWidth, 5000,
                       "and the fix row's coordinate cell follows it")
            for (const key of columnWidthKeys) {
                compare(Number(fixColumnSettingsId.value(key, 0)), 0,
                        key + " holds the default sentinel after the reset")
            }

            menuArea = openColumnHeaderMenu(coordinateTitle)
            resetItem = menuItemNamed(menuArea, "resetColumnWidthsMenuItem")
            verify(!resetItem.enabled, "the reset is disabled again once every width is the default")
            menuArea.menu.close()
            tryVerify(() => !menuArea.menu.visible, 5000, "the header menu closes")
        }

        // A larger font gives a page built afterwards wider default columns,
        // while a width the user dragged stays as stored.
        function test_defaultColumnWidthsGrowWithTheFontScale() {
            const fontSettings = RootData.settings.fontSettings
            fontSettings.fontBaseSize = fontSettings.defaultFontBaseSize
            gotoFixStations()
            waitForRendering(RootData.pageView.currentPageItem)
            const smallWidth = rootId.headerCell("Input CS").columnWidth

            fontSettings.fontBaseSize = fontSettings.defaultFontBaseSize + largerFontStep
            RootData.newProject()
            gotoFixStations()
            const page = RootData.pageView.currentPageItem
            waitForRendering(page)
            let header = null
            tryVerify(() => {
                header = rootId.headerCell("Input CS")
                return header !== null
            }, 5000, "the rebuilt page has an Input CS header")
            verify(header.columnWidth > smallWidth,
                   "the default Input CS width grows with the font, from " + smallWidth
                   + " to " + header.columnWidth)

            fontSettings.fontBaseSize = fontSettings.defaultFontBaseSize
            tryVerify(() => !page.isNarrow, 5000, "the page shows its wide layout")
            waitForRendering(page)
            const startWidth = header.width
            dragColumnEdge("Input CS", -Theme.tableColumnMinimumWidth)
            tryVerify(() => header.width < startWidth, 5000, "the Input CS column narrows")
            const draggedWidth = header.columnWidth

            fontSettings.fontBaseSize = fontSettings.defaultFontBaseSize + largerFontStep
            tryVerify(() => Theme.fontScale > fontSettings.defaultFontBaseSize / 16.0, 5000,
                      "the font scale grows")
            compare(header.columnWidth, draggedWidth, "a dragged width stays as stored")
            compare(Number(fixColumnSettingsId.value("inputCSColumnWidth", 0)), draggedWidth,
                    "and its setting is unchanged")
        }

        function test_fixStationsLinkCount() {
            const cave = gotoFixStations()
            cave.fixStations.addFixStation()
            cave.fixStations.addFixStation()

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Node=" + String(cave.name)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      5000, "should land on cavePage")

            const link = findChild(RootData.pageView.currentPageItem, "fixStationsLink")
            tryCompare(link, "text", "2")

            cave.fixStations.removeAt(0)
            tryCompare(link, "text", "1")
        }
    }

    // The tree's rows on the project's page, checked against the model as the
    // tree opens, closes and changes shape.
    ExternalCenterlineTestCase {
        name: "FixStationPageTreeRows"
        when: windowShown

        function init() {
            rootId.closeAnyOpenEditor()
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        //! Opens the project's Fix Stations page and waits for \a rowCount
        //! rows, returning the tree view.
        function openProjectFixStations(rowCount) {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "the project has a Fix Stations page")
            const view = findChild(RootData.pageView.currentPageItem, "fixStationTreeView")
            verify(view !== null)
            tryCompare(view, "rows", rowCount, 5000, "the tree opens with every row")
            return view
        }

        //! Cave A lists dup, x, dup (the second dup is the one the solve
        //! ignores) and holds folder Sub with s0, s1; cave B lists b0..b3.
        function makeCaves() {
            const a = RootData.region.addNode(null, SurveyNodeKind.Cave, "A")
            const sub = RootData.region.addNode(a, SurveyNodeKind.Folder, "Sub")
            const b = RootData.region.addNode(null, SurveyNodeKind.Cave, "B")
            // A fix on a node with nothing to solve is an unknown station (#727).
            addTripNamingStations(a, ["dup", "x"])
            addTripNamingStations(sub, ["s0", "s1"])
            addTripNamingStations(b, ["b0", "b1", "b2", "b3"])
            compare(a.fixStations.addFixStation("dup"), 0)
            compare(a.fixStations.addFixStation("x"), 1)
            compare(a.fixStations.addFixStation("dup2"), 2)
            // addFixStation refuses a name the node already lists.
            a.fixStations.setData(a.fixStations.index(2), "dup", FixStationModel.StationNameRole)
            sub.fixStations.addFixStation("s0")
            sub.fixStations.addFixStation("s1")
            for (let i = 0; i < 4; ++i) {
                b.fixStations.addFixStation("b" + i)
            }
            return { a: a, sub: sub, b: b }
        }

        // Project root + A{dup, x, dup} + Sub{s0, s1} + B{b0..b3}.
        readonly property int initialRowCount: 13

        //! The delegate on every row the tree shows, in row order.
        function shownItems(view) {
            const items = []
            for (let row = 0; row < view.rows; ++row) {
                items.push(view.itemAtCell(Qt.point(0, row)))
            }
            return items
        }

        //! The first shown row whose cell differs from the model, as text;
        //! empty when every row matches.
        function firstMismatch(view) {
            const model = view.model
            for (let row = 0; row < view.rows; ++row) {
                const item = view.itemAtCell(Qt.point(0, row))
                if (item === null) {
                    const overflow = view.contentHeight > view.height
                            ? ", the rows overflow the " + view.height + "px view" : ""
                    return "row " + row + " has no delegate" + overflow
                }
                const index = view.modelIndex(Qt.point(0, row))
                if (model.data(index, FixStationTreeModel.RowKindRole) !== FixStationTreeModel.FixRow) {
                    const nodeName = model.data(index, FixStationTreeModel.NodeNameRole)
                    if (item.nodeName !== nodeName) {
                        return "row " + row + " shows node " + item.nodeName + ", model says " + nodeName
                    }
                    if (item.row !== row) {
                        return "node " + nodeName + " on row " + row + " says row " + item.row
                    }
                    continue
                }
                const fixRow = model.data(index, FixStationTreeModel.FixRowRole)
                const stationName = model.data(index, FixStationModel.StationNameRole)
                const stationError = model.data(index, FixStationDiagnosticsModel.StationErrorRole)
                if (item.fixRow !== fixRow) {
                    return "row " + row + " (" + stationName + ") shows page row " + item.fixRow
                            + ", model says " + fixRow
                }
                if (item.stationName !== stationName) {
                    return "row " + row + " shows " + item.stationName + ", model says " + stationName
                }
                if (item.stationError !== stationError) {
                    return "row " + row + " (" + stationName + ") shows error "
                            + JSON.stringify(item.stationError) + ", model says " + JSON.stringify(stationError)
                }
                const cell = findChild(item, "stationCell." + fixRow)
                if (cell === null || cell.text !== stationName) {
                    return "row " + row + " has no stationCell." + fixRow + " reading " + stationName
                }
            }
            return ""
        }

        //! Waits for \a rowCount shown rows that all match the model; a
        //! failure names the first row that differs.
        function verifyRowsMatchModel(view, rowCount, when) {
            const shown = {
                get mismatch() {
                    if (view.rows !== rowCount) {
                        return "the tree shows " + view.rows + " rows, expected " + rowCount
                    }
                    return firstMismatch(view)
                }
            }
            tryCompare(shown, "mismatch", "", 5000, when)
        }

        //! How many shown delegates are absent from \a before.
        function newDelegateCount(view, before) {
            return shownItems(view).filter(item => before.indexOf(item) < 0).length
        }

        //! The first shown row delegate named \a objectName, else null.
        function shownRow(view, objectName) {
            return shownItems(view).find(item => item !== null && item.objectName === objectName) ?? null
        }

        //! The live chevron on node \a nodeName's shown row.
        function shownChevron(view, nodeName) {
            const row = shownRow(view, "nodeRow." + nodeName)
            return row === null ? null : findChild(row, "nodeChevron." + nodeName)
        }

        //! The stationError the first shown "dup" row carries, null when no
        //! row shows dup.
        function firstDupError(view) {
            return shownRow(view, "fixRow.dup")?.stationError ?? null
        }

        //! The page's expand/collapse toggle, checked visible.
        function expandToggle() {
            const button = findChild(RootData.pageView.currentPageItem, "expandToggleButton")
            verify(button !== null, "the page has one expand/collapse toggle")
            verify(button.visible, "the toggle shows while the page's node has node or trip rows under it")
            return button
        }

        //! The shown rows' objectNames in row order.
        function shownNames(view) {
            return shownItems(view).map(item => item === null ? "" : item.objectName)
        }

        //! The page's filter field, checked visible.
        function filterField() {
            const field = findChild(RootData.pageView.currentPageItem, "fixStationFilter")
            verify(field !== null, "the page has a filter field on its Add Fix line")
            verify(field.visible, "the filter field shows")
            return field
        }

        //! Types \a text into \a field one key at a time.
        function typeInto(field, text) {
            mouseClick(field)
            tryVerify(() => field.activeFocus, 5000, "the filter field takes focus")
            for (let i = 0; i < text.length; ++i) {
                keyClick(text[i])
            }
        }

        //! Empties \a field the way a user does, one backspace per character.
        function clearField(field) {
            mouseClick(field)
            tryVerify(() => field.activeFocus, 5000, "the filter field takes focus")
            keyClick(Qt.Key_End)
            const length = field.text.length
            for (let i = 0; i < length; ++i) {
                keyClick(Qt.Key_Backspace)
            }
        }

        // A header click orders the fixes under each node by that column, and
        // the node rows keep their place after their parent's fixes; a second
        // click turns the order around.
        function test_aHeaderClickSortsSiblingFixes() {
            makeCaves()
            const view = openProjectFixStations(initialRowCount)
            verifyRowsMatchModel(view, initialRowCount, "opened")
            const station = rootId.headerCell("Station")
            verify(station !== null, "the page has a Station header")
            verify(station.visible, "the header shows in the wide layout")
            waitForRendering(view)

            mouseClick(station)
            tryCompare({ get names() { return shownNames(view).join(",") } }, "names",
                       ["nodeRow.", "nodeRow.A", "fixRow.dup", "fixRow.dup", "fixRow.x",
                        "nodeRow.Sub", "fixRow.s0", "fixRow.s1",
                        "nodeRow.B", "fixRow.b0", "fixRow.b1", "fixRow.b2", "fixRow.b3"].join(","),
                       5000, "ascending by station, Sub after A's own fixes")
            verifyRowsMatchModel(view, initialRowCount, "after sorting ascending")

            mouseClick(station)
            tryCompare({ get names() { return shownNames(view).join(",") } }, "names",
                       ["nodeRow.", "nodeRow.A", "fixRow.x", "fixRow.dup", "fixRow.dup",
                        "nodeRow.Sub", "fixRow.s1", "fixRow.s0",
                        "nodeRow.B", "fixRow.b3", "fixRow.b2", "fixRow.b1", "fixRow.b0"].join(","),
                       5000, "descending by station, Sub still after A's own fixes")
            verifyRowsMatchModel(view, initialRowCount, "after sorting descending")
        }

        // Typing in the filter field keeps the matching fixes and the node
        // rows above them; clearing it brings every row back, and a node the
        // user closed before filtering is closed again.
        function test_typingAFilterKeepsMatchesAndTheirAncestors() {
            makeCaves()
            const view = openProjectFixStations(initialRowCount)
            mouseClick(shownChevron(view, "B"))
            tryCompare(view, "rows", initialRowCount - 4, 5000, "collapsing B hides its fixes")
            const field = filterField()

            typeInto(field, "s1")
            tryCompare({ get names() { return shownNames(view).join(",") } }, "names",
                       ["nodeRow.", "nodeRow.A", "nodeRow.Sub", "fixRow.s1"].join(","),
                       5000, "only s1 and the node rows above it")
            verifyRowsMatchModel(view, 4, "while filtering on s1")

            clearField(field)
            tryCompare(field, "text", "")
            verifyRowsMatchModel(view, initialRowCount - 4, "after clearing, with B closed again")
            verify(shownRow(view, "fixRow.b0") === null, "B stays closed")
        }

        // Add Fix clears the filter, so the new row it adds is on screen. The
        // picked s1 row names Sub, so the fix lands there.
        function test_addFixClearsTheFilter() {
            const caves = makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const field = filterField()
            typeInto(field, "s1")
            tryCompare(view, "rows", 4, 5000, "the filter keeps s1 and its node rows")

            // Page rows: A's dup, x, dup are 0..2, Sub's s0, s1 are 3, 4.
            const s1PageRow = 4
            const page = RootData.pageView.currentPageItem
            page.currentFixRow = s1PageRow
            tryCompare(view, "currentIndex", s1PageRow, 5000, "s1 is the current row")

            const addButton = findChild(findChild(page, "addFixBar"), "addButton")
            verify(addButton !== null, "the page has an Add Fix button")
            tryVerify(() => addButton.enabled, 5000, "Add is enabled on a row whose node holds fixes")
            mouseClick(addButton)
            tryCompare(field, "text", "", 5000, "adding a fix clears the filter")
            tryCompare(caves.sub.fixStations, "count", 3, 5000, "the fix lands on s1's node")
            compare(page.cave.fixStations.count, 0, "and none on the project")
            verifyRowsMatchModel(view, initialRowCount + 1, "every row plus the new one")
        }

        //! Clicks node row \a nodeName's background with \a button, right of
        //! its name and chips and inside the view.
        function clickNodeRow(view, nodeName, button) {
            const row = shownRow(view, "nodeRow." + nodeName)
            verify(row !== null, "node row " + nodeName + " is shown")
            const at = row.mapFromItem(view, view.width * 3 / 4, 0)
            mouseClick(row, at.x, row.height / 2, button)
            return row
        }

        //! The live station cell of page row \a fixRow, else null.
        function shownStationCell(view, fixRow) {
            for (const item of shownItems(view)) {
                const cell = item === null ? null : findChild(item, "stationCell." + fixRow)
                if (cell !== null) {
                    return cell
                }
            }
            return null
        }

        //! Right-clicks node row \a nodeName and returns its menu once open.
        function openNodeRowMenu(view, nodeName) {
            const row = shownRow(view, "nodeRow." + nodeName)
            verify(row !== null, "node row " + nodeName + " is shown")
            const rowMenu = findChild(row, "nodeRowMenu." + nodeName)
            verify(rowMenu !== null, "node row " + nodeName + " has a right-click menu")
            clickNodeRow(view, nodeName, Qt.RightButton)
            tryVerify(() => rowMenu.menu !== null && rowMenu.menu.opened, 5000,
                      "a right-click opens node row " + nodeName + "'s menu")
            return rowMenu.menu
        }

        //! \a menu's "Add fix station" item, else null.
        function addFixMenuItemOf(menu) {
            for (let i = 0; i < menu.count; ++i) {
                if (menu.itemAt(i).objectName === "addFixMenuItem") {
                    return menu.itemAt(i)
                }
            }
            return null
        }

        // The project's own node holds no fixes, so its page's empty state
        // says to pick a node, and Add waits for one.
        function test_theProjectPagesEmptyStateSaysToSelectANode() {
            openProjectFixStations(1)
            const page = RootData.pageView.currentPageItem
            const help = findChild(page, "noFixStationsHelpBox")
            verify(help !== null, "the page has an empty-state box")
            tryVerify(() => help.visible, 5000, "the box shows with no fixes")
            verify(String(help.text).indexOf("Select a cave, folder, or section") >= 0,
                   "the box says to select a node, got: " + help.text)
            const addButton = findChild(findChild(page, "addFixBar"), "addButton")
            tryVerify(() => !addButton.enabled, 5000, "Add is disabled on the project's page")
        }

        // On the project's page, Add waits for a picked row; a cave row makes
        // it add to that cave, opening the cave to show the new fix.
        function test_addOnTheProjectPageTargetsThePickedNodeRow() {
            const caves = makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const page = RootData.pageView.currentPageItem
            const addBar = findChild(page, "addFixBar")
            const addButton = findChild(addBar, "addButton")
            verify(addButton !== null, "the page has an Add Fix button")

            tryVerify(() => !addButton.enabled, 5000, "Add is disabled with no row picked")
            verify(String(addBar.addToolTip).startsWith("Select"),
                   "the tooltip says to select a node, got: " + addBar.addToolTip)
            mouseMove(addButton, addButton.width / 2, addButton.height / 2)
            let tip = null
            tryVerify(() => {
                tip = findChild(addBar, "addToolTip")
                return tip !== null && tip.visible
            }, 5000, "hovering the disabled Add shows its tooltip")
            compare(tip.text, addBar.addToolTip)
            rootId.leaveToolTip(this, tip)

            mouseClick(shownChevron(view, "A"))
            tryCompare(view, "rows", initialRowCount - 6, 5000, "closing A hides its rows")

            clickNodeRow(view, "A", Qt.LeftButton)
            tryVerify(() => view.currentNode === caves.a, 5000, "a click on A's row picks it")
            compare(page.currentFixRow, -1, "a node row picks no fix")
            compare(view.currentIndex, -1)
            tryVerify(() => addButton.enabled, 5000, "Add is enabled on a cave row")
            compare(addBar.addToolTip, "Add a fix station to A")

            mouseClick(addButton)
            tryCompare(caves.a.fixStations, "count", 4, 5000, "the new fix lands on A")
            compare(page.cave.fixStations.count, 0, "and none on the project")
            // A's own fixes come first, so the new one is page row 3.
            const newPageRow = 3
            tryVerify(() => shownRow(view, "nodeRow.A")?.expanded === true, 5000,
                      "A opens to show the new fix")
            tryVerify(() => shownStationCell(view, newPageRow) !== null, 5000,
                      "the new fix's row is shown")
            compare(view.currentNode, caves.a, "A stays the picked row")
        }

        // A picked node row is no fix: currentFixRow reads -1 on it, and a
        // link to a fix picks the fix again.
        function test_aPickedNodeRowReadsAsNoFix() {
            const caves = makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const page = RootData.pageView.currentPageItem
            // Page row 1 is A's x, page row 5 is B's b0.
            page.currentFixRow = 1
            tryCompare(view, "currentIndex", 1, 5000, "the link picks x")

            clickNodeRow(view, "B", Qt.LeftButton)
            tryVerify(() => view.currentNode === caves.b, 5000, "a click on B's row picks it")
            tryCompare(page, "currentFixRow", -1, 5000, "B's row picks no fix")
            compare(view.currentIndex, -1)

            page.currentFixRow = 5
            tryCompare(view, "currentIndex", 5, 5000, "a link picks b0")
            tryVerify(() => view.currentNode === null, 5000, "and the node row lets go")
        }

        // A node row's menu adds a fix to that node; on the project's row the
        // item is disabled.
        function test_aNodeRowsMenuAddsAFixToThatNode() {
            const caves = makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const page = RootData.pageView.currentPageItem

            const menu = openNodeRowMenu(view, "Sub")
            const item = addFixMenuItemOf(menu)
            verify(item !== null, "Sub's menu offers to add a fix station")
            compare(item.text, "Add fix station")
            verify(item.enabled, "Sub can hold a fix")
            mouseClick(item)
            tryCompare(caves.sub.fixStations, "count", 3, 5000, "the fix lands on Sub")
            tryVerify(() => !menu.visible, 5000, "choosing the item closes the menu")
            compare(page.cave.fixStations.count, 0, "and none on the project")

            const rootMenu = openNodeRowMenu(view, "")
            const rootItem = addFixMenuItemOf(rootMenu)
            verify(rootItem !== null, "the project's row offers the item too")
            verify(!rootItem.enabled, "the project's row holds no fixes")
            rootMenu.close()
            tryVerify(() => !rootMenu.visible)
        }

        // A fix row's menu offers to add a fix to the node that owns the row.
        function test_aFixRowsMenuAddsAFixToItsNode() {
            const caves = makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const page = RootData.pageView.currentPageItem
            const row = shownRow(view, "fixRow.s1")
            verify(row !== null, "s1's row is shown")
            const rowMenu = findChild(row, "fixRowMenu." + row.fixRow)
            verify(rowMenu !== null, "s1's row has a right-click menu")
            const cell = shownStationCell(view, row.fixRow)
            verify(cell !== null, "s1's row has a station cell")

            mouseClick(cell, cell.width / 2, cell.height / 2, Qt.RightButton)
            tryVerify(() => rowMenu.menu !== null && rowMenu.menu.opened, 5000,
                      "a right-click opens s1's menu")
            const menu = rowMenu.menu
            compare(menu.count, 2, "the menu holds Remove and Add")
            compare(menu.itemAt(1).text, "Add fix station to Sub")
            mouseClick(menu.itemAt(1))
            tryCompare(caves.sub.fixStations, "count", 3, 5000, "the fix lands on Sub")
            compare(page.cave.fixStations.count, 0, "and none on the project")
        }

        // A link to a fix the filter hides clears the filter and shows the fix.
        function test_aLinkToAFilteredRowClearsTheFilter() {
            const caves = makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const field = filterField()
            typeInto(field, "s1")
            tryCompare(view, "rows", 4, 5000, "the filter keeps s1 and its node rows")

            // Page rows: A's dup, x, dup are 0..2, Sub's s0, s1 are 3, 4, B's b0..b3 are 5..8.
            const b2PageRow = 7
            RootData.pageView.currentPageItem.currentFixRow = b2PageRow
            tryCompare(field, "text", "", 5000, "a link to a hidden row clears the filter")
            tryVerify(() => shownRow(view, "fixRow.b2") !== null, 5000, "b2 is shown")
        }

        // A node's name is the link to its Fix Stations page; the page's own
        // row names the page already open, so it stays plain text.
        function test_aNodeNameOpensItsFixStationsPage() {
            const caves = makeCaves()
            const view = openProjectFixStations(initialRowCount)

            const rootRow = shownRow(view, "nodeRow.")
            verify(rootRow !== null, "the project's own row is shown")
            const rootLink = rootId.findChildWithPrefix(rootRow, "nodeNameLink.")
            verify(rootLink === null || !rootLink.visible,
                   "the page's own row shows no link to itself")
            verify(rootId.showsText(rootRow, "Project"), "the page's own row still names its node")

            const rowA = shownRow(view, "nodeRow.A")
            verify(rowA !== null, "A's row is shown")
            verify(!rootId.showsText(rowA, "Open page"), "the row carries no separate Open page link")
            const link = findChild(rowA, "nodeNameLink.A")
            verify(link !== null, "A's name is a link")
            verify(link.visible, "A's name link is shown")
            compare(link.text, "A")

            mouseClick(link)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage"
                            && RootData.pageView.currentPageItem.cave === caves.a,
                      5000, "the link opens A's Fix Stations page")
            verify(RootData.pageSelectionModel.currentPageAddress.endsWith("Node=A/Fix Stations"),
                   "the address names A's Fix Stations page, got "
                   + RootData.pageSelectionModel.currentPageAddress)
        }

        // A node row counts only the fixes the node itself lists.
        function test_aNodeRowCountsOnlyItsOwnFixes() {
            makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const rowA = shownRow(view, "nodeRow.A")
            verify(rowA !== null, "A's row is shown")
            tryVerify(() => rootId.showsText(rowA, "3 fixes"), 5000, "A counts its own three fixes")
            verify(!rootId.showsText(rowA, "with children"), "A's row carries no subtree count")
        }

        // A node with no fixes of its own shows one chip, "No fixed stations".
        function test_aFixlessNodeRowShowsOneNoFixedStationsChip() {
            makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const rootRow = shownRow(view, "nodeRow.")
            verify(rootRow !== null, "the project root's row is shown")
            tryVerify(() => rootId.showsText(rootRow, "No fixed stations"), 5000,
                      "the root row says it has no fixed stations")
            verify(!rootId.showsText(rootRow, "0 fixes"), "the root row shows no \"0 fixes\" chip")

            const matches = rootId.itemsShowingText(rootRow, "No fixed stations")
            compare(matches.length, 1, "one element says \"No fixed stations\"")
            verify(matches[0].background !== null, "the one element is the count chip")

            const rowA = shownRow(view, "nodeRow.A")
            verify(rowA !== null, "A's row is shown")
            tryVerify(() => rootId.showsText(rowA, "3 fixes"), 5000, "A still counts its own fixes")
            verify(!rootId.showsText(rowA, "No fixed stations"), "a row with fixes shows no empty-state text")
        }

        //! The page's help box named \a objectName, checked present.
        function pageHelpBox(objectName) {
            const help = findChild(RootData.pageView.currentPageItem, objectName)
            verify(help !== null, objectName + " must exist")
            return help
        }

        // While a filter is active, a node row counts the fixes the filter
        // shows out of its own; a node with none at all keeps its one
        // "No fixed stations" chip.
        function test_aFilteredNodeRowCountsTheFixesTheFilterShows() {
            makeCaves()
            const view = openProjectFixStations(initialRowCount)
            typeInto(filterField(), "s1")
            tryCompare(view, "rows", 4, 5000, "the filter keeps s1 and its node rows")

            const rowA = shownRow(view, "nodeRow.A")
            verify(rowA !== null, "A's row is shown")
            tryVerify(() => rootId.showsText(rowA, "0 of 3 fixes"), 5000,
                      "A shows none of its three fixes, got \""
                      + rootId.itemsShowingText(rowA, "fix").map(item => item.text).join("\", \"") + "\"")
            const rowSub = shownRow(view, "nodeRow.Sub")
            verify(rowSub !== null, "Sub's row is shown")
            tryVerify(() => rootId.showsText(rowSub, "1 of 2 fixes"), 5000,
                      "Sub shows one of its two fixes, got \""
                      + rootId.itemsShowingText(rowSub, "fix").map(item => item.text).join("\", \"") + "\"")

            const rootRow = shownRow(view, "nodeRow.")
            verify(rootRow !== null, "the project root's row is shown")
            compare(rootId.itemsShowingText(rootRow, "No fixed stations").length, 1,
                    "the fixless root keeps one \"No fixed stations\" chip")
            verify(!rootId.showsText(rootRow, " of "), "the fixless root shows no filtered count")
        }

        // A filter that matches nothing says so, names the filter and offers
        // to clear it; the project still has fixes, so the empty-project box
        // stays hidden.
        function test_aFilterThatMatchesNothingSaysSoAndClears() {
            makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const field = filterField()
            typeInto(field, "zzz")
            tryCompare(view, "rows", 1, 5000, "only the page's own row is left")

            const noMatches = pageHelpBox("noFilterMatchesHelpBox")
            tryVerify(() => noMatches.visible, 5000, "the no-match box shows")
            verify(noMatches.text.indexOf("zzz") >= 0,
                   "the no-match box names the filter, got \"" + noMatches.text + "\"")
            verify(!pageHelpBox("noFixStationsHelpBox").visible,
                   "the project has fixes, so the empty-project box stays hidden")

            noMatches.linkActivated("clear")
            tryCompare(field, "text", "", 5000, "the link clears the filter")
            tryVerify(() => !noMatches.visible, 5000, "the no-match box hides once the filter clears")
            verifyRowsMatchModel(view, initialRowCount, "every row is back")
        }

        // Clearing the filter puts every node row back on its whole count.
        function test_clearingTheFilterRestoresTheWholeCounts() {
            makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const field = filterField()
            typeInto(field, "s1")
            tryCompare(view, "rows", 4, 5000, "the filter keeps s1 and its node rows")
            const filteredA = shownRow(view, "nodeRow.A")
            verify(filteredA !== null, "A's row is shown")
            tryVerify(() => rootId.showsText(filteredA, "0 of 3 fixes"), 5000,
                      "A counts its filtered fixes first")

            clearField(field)
            tryCompare(field, "text", "")
            verifyRowsMatchModel(view, initialRowCount, "every row is back")

            const expected = { "A": "3 fixes", "Sub": "2 fixes", "B": "4 fixes" }
            for (const name in expected) {
                tryVerify(() => {
                    const row = shownRow(view, "nodeRow." + name)
                    return row !== null && rootId.showsText(row, expected[name])
                }, 5000, name + " shows " + expected[name])
            }
            for (const name of ["", "A", "Sub", "B"]) {
                tryVerify(() => {
                    const row = shownRow(view, "nodeRow." + name)
                    return row !== null && !rootId.showsText(row, " of ")
                }, 5000, "node row \"" + name + "\" shows no filtered count")
            }
        }

        // One button collapses every node below the page's own row and expands
        // them again; it follows a single node's chevron too.
        function test_oneButtonTogglesExpandAndCollapseAll() {
            makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const button = expandToggle()
            compare(button.text, "Collapse all", "with every node open the button collapses")
            const collapseIcon = String(button.icon.source)
            verify(collapseIcon.indexOf("arrows-collapse") >= 0, "the button shows the collapse icon")

            mouseClick(button)
            tryCompare(view, "rows", 3, 5000, "collapse all leaves the project, A and B")
            tryCompare(button, "text", "Expand all", 5000, "with nodes closed the button expands")
            verify(String(button.icon.source).indexOf("arrows-expand") >= 0,
                   "the icon swaps to the expand icon")

            mouseClick(button)
            tryCompare(view, "rows", initialRowCount, 5000, "expand all opens every node")
            tryCompare(button, "text", "Collapse all")
            compare(String(button.icon.source), collapseIcon)

            mouseClick(shownChevron(view, "B"))
            tryCompare(view, "rows", initialRowCount - 4, 5000, "collapsing B hides its fixes")
            tryCompare(button, "text", "Expand all", 5000,
                       "one closed node turns the button to expand all")
        }

        // Deleting the only collapsed node leaves nothing to expand, so the
        // page drops its id and the toggle offers collapse again.
        function test_deletingTheOnlyCollapsedNodeClearsTheExpandAllState() {
            const caves = makeCaves()
            const view = openProjectFixStations(initialRowCount)
            const page = RootData.pageView.currentPageItem
            const button = expandToggle()

            mouseClick(shownChevron(view, "B"))
            tryCompare(view, "rows", initialRowCount - 4, 5000, "collapsing B hides its fixes")
            tryCompare(page, "hasCollapsedNodes", true, 5000, "B is the one closed node")
            tryCompare(button, "text", "Expand all")

            const root = RootData.region.rootNode
            root.removeNode(root.indexOfNode(caves.b))
            tryCompare(view, "rows", initialRowCount - 5, 5000, "B's row leaves the tree")

            tryCompare(page, "hasCollapsedNodes", false, 5000,
                       "with B gone no node is closed")
            tryCompare(expandToggle(), "text", "Collapse all", 5000,
                       "the toggle offers collapse all again")
        }

        // The page's toggle is the shared ExpandCollapseButton the survey
        // tree uses: it carries the collapsed-rows flag and the two requests.
        function test_theToggleIsTheSharedExpandCollapseButton() {
            makeCaves()
            openProjectFixStations(initialRowCount)
            const button = expandToggle()
            verify(button instanceof ExpandCollapseButton,
                   "the page's toggle is the shared ExpandCollapseButton")
            compare(typeof button.hasCollapsedRows, "boolean",
                    "the toggle reads its state from the shared hasCollapsedRows")
            compare(button.hasCollapsedRows, false, "with every node open nothing is collapsed")
            compare(typeof button.expandAllRequested, "function",
                    "the toggle asks for expand all through the shared signal")
            compare(typeof button.collapseAllRequested, "function",
                    "the toggle asks for collapse all through the shared signal")
        }

        // Closing and reopening a cave hands its rows back to the delegates
        // the view already built, so a toggle costs no new delegates.
        function test_aCollapseAndExpandReusesTheRowDelegates() {
            makeCaves()
            const view = openProjectFixStations(initialRowCount)
            verifyRowsMatchModel(view, initialRowCount, "opened")
            const before = shownItems(view)

            mouseClick(shownChevron(view, "A"))
            tryCompare(view, "rows", initialRowCount - 6, 5000, "collapsing A hides its rows")
            mouseClick(shownChevron(view, "A"))
            verifyRowsMatchModel(view, initialRowCount, "after collapsing and expanding A")

            compare(newDelegateCount(view, before), 0,
                    "every row A shows again sits on a delegate the view already built")
        }

        // The rows the tree shows match the model after every change in its
        // shape, on delegates the view keeps: a removed or added fix shifts
        // the page rows below it, and a pooled delegate carries the shift
        // too.
        function test_everyShownRowMatchesTheModelAfterTheTreeChanges() {
            const caves = makeCaves()
            const view = openProjectFixStations(initialRowCount)
            verifyRowsMatchModel(view, initialRowCount, "opened")
            verify(firstDupError(view) === "", "the first dup is the one that counts")
            verify(shownItems(view).some(item => item !== null && item.objectName === "fixRow.dup"
                                         && item.stationError !== ""),
                   "the second dup is flagged")

            mouseClick(shownChevron(view, "A"))
            tryCompare(view, "rows", initialRowCount - 6, 5000, "collapsing A hides its rows")
            mouseClick(shownChevron(view, "A"))
            verifyRowsMatchModel(view, initialRowCount, "after collapsing and expanding A")

            // Removing the first dup moves every row below it up one page row,
            // and the second dup stops being a duplicate.
            let before = shownItems(view)
            caves.a.fixStations.removeAt(0)
            let rowCount = initialRowCount - 1
            verifyRowsMatchModel(view, rowCount, "after removing the first dup")
            compare(firstDupError(view), "", "the surviving dup carries no error")
            compare(newDelegateCount(view, before), 0,
                    "removing a row keeps every other row's delegate")

            before = shownItems(view)
            caves.a.fixStations.addFixStation("a9")
            rowCount += 1
            verifyRowsMatchModel(view, rowCount, "after adding a9")
            verify(newDelegateCount(view, before) <= 1,
                   "adding a row builds at most its own delegate, built "
                   + newDelegateCount(view, before))

            // A node inserted ahead of Sub shifts Sub's page rows down.
            before = shownItems(view)
            const early = RootData.region.addNode(caves.a, SurveyNodeKind.Folder, "Early")
            RootData.region.moveNode(early, caves.a, 0)
            early.fixStations.addFixStation("e0")
            rowCount += 2
            verifyRowsMatchModel(view, rowCount, "after inserting Early with e0")
            verify(newDelegateCount(view, before) <= 2,
                   "inserting a node and its fix builds at most their two delegates, built "
                   + newDelegateCount(view, before))
        }
    }

    ExternalCenterlineTestCase {
        name: "FixStationPageAttachedFixes"
        when: windowShown

        readonly property string subtreeFixture:
            "external-centerlines/survex_subtree_fixes/ridge.svx"
        readonly property int subtreeFixCount: 4
        // A tree tall enough to scroll, where every cave's own fixes meet
        // the read-only rows its attachment lists.
        readonly property int mixedCaveCount: 10
        readonly property int mixedOwnFixCount: 5
        readonly property string mixedOwnPrefix: "own"
        readonly property int sweepStep: 40
        readonly property int sweepMaxSteps: 500
        readonly property int toggleRuns: 3
        readonly property int toggledCaveCount: 2
        readonly property list<string> fixControlPrefixes: ["readOnlyLock.", "fixRowMenu.", "pickFromViewButton."]

        function init() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function cleanup() {
            RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.System
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.project.newProject()
        }

        //! Whether \a item sits somewhere under \a ancestor.
        function isInside(item, ancestor) {
            for (let up = item; up !== null; up = up.parent) {
                if (up === ancestor) {
                    return true
                }
            }
            return false
        }

        //! \a objectName on a row the tree shows, else elsewhere on the page.
        //! A delegate the view pools keeps the names it last carried, so a
        //! match under the view's content that no shown row holds is a pooled
        //! row's and counts as absent.
        function liveChild(objectName) {
            const page = RootData.pageView.currentPageItem
            if (page === null) {
                return null
            }
            const view = findChild(page, "fixStationTreeView")
            if (view !== null) {
                for (let row = 0; row < view.rows; ++row) {
                    const rowItem = view.itemAtCell(Qt.point(0, row))
                    if (rowItem === null) {
                        continue
                    }
                    if (rowItem.objectName === objectName) {
                        return rowItem
                    }
                    const found = findChild(rowItem, objectName)
                    if (found !== null) {
                        return found
                    }
                }
            }
            const found = findChild(page, objectName)
            if (found !== null && view !== null && isInside(found, view.contentItem)) {
                return null
            }
            return found
        }

        //! Waits for \a objectName on the current page's shown rows and
        //! returns it.
        function waitForChild(objectName) {
            let item = null
            tryVerify(() => {
                item = liveChild(objectName)
                return item !== null && item.visible
            }, 5000, objectName + " should be on screen")
            return item
        }

        //! Opens the bare-fix system picker of the owner named \a ownerName:
        //! at rest its node row shows the system as text, and a double-click on
        //! that text loads the picker. Returns the picker.
        function openOwnerPicker(ownerName) {
            const label = waitForChild("bareFixSystemValue." + ownerName)
            waitForRendering(rootId)
            doubleTap(label)
            return waitForChild("bareFixSystemCombo." + ownerName)
        }

        //! A new cave named \a caveName with \a tripCount trips, named t0, t1, ….
        function makeManyTripCave(caveName, tripCount) {
            const cave = RootData.region.addNode(null, SurveyNodeKind.Cave, caveName)
            for (let i = 0; i < tripCount; ++i) {
                cave.addTrip()
                cave.trip(i).name = "t" + i
            }
            return cave
        }

        //! How many attached fixes \a node and its descendants list: a file's
        //! fix sits on the derived node whose file holds it.
        function subtreeAttachedFixCount(node) {
            let count = node.attachedFixes.count
            for (let i = 0; i < node.tripCount; ++i) {
                count += node.trip(i).attachedFixes.count
            }
            for (let i = 0; i < node.childNodeCount; ++i) {
                count += subtreeAttachedFixCount(node.childNode(i))
            }
            return count
        }

        // The attached file's own *fix is a row of the page after the node's
        // own, read-only, named the way a node fix on that station is spelled.
        function test_attachedFileFixesAreReadOnlyRowsOfTheTable() {
            const cave = makeSavedCaveAttach("fix-station-attached-fixes",
                                             "external-centerlines/survex_blocks.svx")
            tryVerify(() => subtreeAttachedFixCount(cave) === 1, 20000, "the scan lists the file's fix")
            cave.fixStations.addFixStation()
            const model = cave.fixStations
            model.setData(model.index(0), "EPSG:32613", FixStationModel.InputCSRole)
            // A name gives the cell text to double-click.
            model.setData(model.index(0), "doghill.d2", FixStationModel.StationNameRole)

            RootData.pageSelectionModel.currentPageAddress =
                "Source/Data/Node=" + String(cave.name) + "/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should land on fixStationPage")
            const page = RootData.pageView.currentPageItem
            const tableView = findChild(page, "fixStationTreeView")
            tryCompare(tableView, "count", 2)
            compare(findChild(page, "noFixStationsHelpBox").visible, false)

            // The qualified name: the file writes d1 inside *begin doghill.
            const attachedStation = waitForChild("stationCell.1")
            compare(attachedStation.text, "doghill.d1")
            // The file names no system for d1, so it is placed at the origin.
            compare(waitForChild("noCoordinateSystemBadge.1").text, "none \u2192 origin")
            const attachedCoordinate = waitForChild("coordinateCell.1")
            compare(attachedCoordinate.text, "0, 0, 0.000m")
            verify(waitForChild("readOnlyLock.1") !== null, "the row carries the lock")
            compare(liveChild("readOnlyLock.0"), null, "the node's own row does not")
            compare(liveChild("inputCSComboBox.1"), null, "nor a system to pick")
            compare(liveChild("inputCSLabel.1"), null, "nor a system to double-click")
            compare(liveChild("pickFromViewButton.1"), null, "nor a pick button")
            verify(waitForChild("pickFromViewButton.0") !== null)

            // Focus opens an editor the way a double-click does (QTest's
            // double-click sequence never reaches the field's doubleTapped),
            // and it opens the node's row, so the read-only row's refusal
            // below is the row's doing, not the gesture's.
            const ownStation = waitForChild("stationCell.0")
            ownStation.forceActiveFocus()
            tryVerify(() => ownStation.isEditting, 5000, "the node's own row opens an editor")
            rootId.closeAnyOpenEditor()
            tryVerify(() => !ownStation.isEditting)

            for (const cell of [attachedStation, attachedCoordinate]) {
                mouseDoubleClickSequence(cell)
                cell.forceActiveFocus()
                cell.openEditor()
                verify(!cell.isEditting, cell.objectName + " opens no editor")
            }
            compare(rootId.shadowEditor.coreClickInput, null, "and the shared editor stays closed")

            // The right-click menu is the remove path; the attached row has none.
            mouseClick(attachedStation, 2, 2, Qt.RightButton)
            compare(liveChild("fixRowMenu.1"), null, "the attached row offers no Remove")
            verify(liveChild("fixRowMenu.0") !== null, "the node's own row does")

            // The node's row still edits with the attached row beside it.
            const ownCoordinate = waitForChild("coordinateCell.0")
            ownCoordinate.openEditor()
            rootId.shadowEditor.setEditorText("478000, 4430000, 1655m")
            compare(ownCoordinate.commitChanges(), true, "the edit commits")
            fuzzyCompare(model.data(model.index(0), FixStationModel.EastingRole), 478000, 1e-6)
            fuzzyCompare(model.data(model.index(0), FixStationModel.NorthingRole), 4430000, 1e-6)
            tryCompare(tableView, "count", 2)

            // And removes; the attached row moves up to index 0.
            const removeAskBox = findChild(page, "removeChallange")
            removeAskBox.indexToRemove = 0
            removeAskBox.removeName = ""
            removeAskBox.show()
            tryVerify(() => removeAskBox.visible)
            mouseClick(findChild(removeAskBox, "removeButton"))
            tryVerify(() => !removeAskBox.visible)
            tryCompare(model, "count", 0)
            tryCompare(tableView, "count", 1)
            tryCompare(waitForChild("stationCell.0"), "text", "doghill.d1")
            verify(waitForChild("readOnlyLock.0") !== null, "the attached row is now row 0")
            compare(subtreeAttachedFixCount(cave), 1, "and is still the file's")
            compare(findChild(page, "noFixStationsHelpBox").visible, false,
                    "the table is not empty while the file's fix is listed")

            // The narrow layout marks the row with the lock alone, as the wide
            // one does; the lock's tooltip names the file.
            const wideWidth = rootId.width
            rootId.width = Theme.breakpointPanelCollapse / 2
            const narrowRow = waitForChild("narrowFixRow.0")
            const narrowLock = waitForChild("readOnlyLock.0")
            const narrowTip = rootId.hoverSharedToolTip(this, narrowLock)
            verify(narrowTip.text.indexOf("survex_blocks.svx") >= 0,
                   "the narrow lock's tooltip names the file: \"" + narrowTip.text + "\"")
            rootId.leaveToolTip(this, narrowTip)
            const wordedSource = Array.from(narrowRow.children).filter(
                                     child => child instanceof QC.Label
                                     && String(child.text).indexOf("survex_blocks.svx") >= 0)
            compare(wordedSource.length, 0, "the narrow row names the file only in the lock's tooltip")
            compare(liveChild("inputCSComboBox.0"), null)
            compare(waitForChild("noCoordinateSystemBadge.0").text, "none \u2192 origin")
            compare(liveChild("pickFromViewButton.0"), null)
            rootId.width = wideWidth
        }

        //! The Fix Stations page's tree at \a address, once the page is current.
        function openFixStationTree(address) {
            RootData.pageSelectionModel.currentPageAddress = address
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should land on fixStationPage at " + address)
            return findChild(RootData.pageView.currentPageItem, "fixStationTreeView")
        }

        //! The choice of \a chooser whose roleValue is \a value, null for none.
        function delegateChoiceFor(chooser, value) {
            const choices = chooser.choices
            for (let i = 0; i < choices.length; ++i) {
                if (choices[i].roleValue === value) {
                    return choices[i]
                }
            }
            return null
        }

        //! Checks that \a view's current chooser, the \a layout one, picks by
        //! delegateKind and gives each of the four kinds a delegate of its own.
        function verifyOneDelegatePerKind(view, layout) {
            const chooser = view.delegate
            compare(chooser.role, "delegateKind", "the " + layout + " chooser picks by delegateKind")
            const kinds = [FixStationTreeModel.NodeDelegate, FixStationTreeModel.TripDelegate,
                           FixStationTreeModel.EditableFix, FixStationTreeModel.ReadOnlyFix]
            const delegates = kinds.map(kind => {
                verify(kind !== undefined, "every delegate kind is a FixStationTreeModel value")
                const choice = delegateChoiceFor(chooser, kind)
                verify(choice !== null, "the " + layout + " chooser has a choice for kind " + kind)
                verify(choice.delegate !== null, "and that choice has a delegate")
                return choice.delegate
            })
            for (let i = 0; i < delegates.length; ++i) {
                for (let j = i + 1; j < delegates.length; ++j) {
                    verify(delegates[i] !== delegates[j],
                           "the " + layout + " kinds " + kinds[i] + " and " + kinds[j]
                           + " each have their own delegate")
                }
            }
        }

        // A node's own, editable fix rows and its attached, read-only ones
        // come from separate delegates in both layouts, so the view pools
        // each kind apart and a reused delegate never switches kind.
        function test_fixRowKindsEachHaveTheirOwnDelegate() {
            const cave = RootData.region.addNode(null, SurveyNodeKind.Cave, "Kinds")
            cave.fixStations.addFixStation("k1")
            const view = openFixStationTree("Source/Data/Node=Kinds/Fix Stations")
            tryVerify(() => view.rows > 0, 5000, "the tree shows the cave's fix")

            const wideWidth = rootId.width
            try {
                verifyOneDelegatePerKind(view, "wide")
                const wideChooser = view.delegate
                rootId.width = Theme.breakpointPanelCollapse / 2
                tryVerify(() => view.delegate !== wideChooser, 5000, "the narrow layout has its own chooser")
                verifyOneDelegatePerKind(view, "narrow")
            } finally {
                rootId.width = wideWidth
            }
        }

        //! Saves the project and builds mixedCaveCount caves, each with
        //! mixedOwnFixCount fixes of its own and one trip that attaches
        //! bare_fix.svx, so every cave's own rows meet read-only rows.
        //! Returns the number of fix rows the caves hold.
        function makeMixedFixCaves(projectBaseName) {
            saveProjectAs(projectBaseName)
            const source = TestHelper.testcasesDatasetPath(
                             "external-centerlines/survex_bare_fix/bare_fix.svx")
            const attachedCount = (cave) => {
                let count = cave.attachedFixes.count
                for (let i = 0; i < cave.tripCount; ++i) {
                    count += cave.trip(i).attachedFixes.count
                }
                return count
            }
            let fixRows = 0
            for (let c = 0; c < mixedCaveCount; ++c) {
                const cave = RootData.region.addNode(null, SurveyNodeKind.Cave, "M" + c)
                cave.addTrip()
                for (let f = 0; f < mixedOwnFixCount; ++f) {
                    cave.fixStations.addFixStation(mixedOwnPrefix + c + "_" + f)
                }
                attachSourceToTrip(cave.trip(0), source)
                tryVerify(() => attachedCount(cave) > 0, 20000, "the scan lists M" + c + "'s attached fix")
                fixRows += mixedOwnFixCount + attachedCount(cave)
            }
            return fixRows
        }

        //! Scrolls \a view from the top to the bottom and back in sweepStep
        //! steps, laying it out and calling \a visit after every step.
        //! Returns the number of steps.
        function sweep(view, visit) {
            let steps = 0
            while (view.contentY + view.height < view.contentHeight - 1 && steps < sweepMaxSteps) {
                view.contentY = Math.min(view.contentY + sweepStep, view.contentHeight - view.height)
                view.forceLayout()
                visit()
                ++steps
            }
            while (view.contentY > 0 && steps < 2 * sweepMaxSteps) {
                view.contentY = Math.max(view.contentY - sweepStep, 0)
                view.forceLayout()
                visit()
                ++steps
            }
            return steps
        }

        //! Collapses and expands \a view's root toggleRuns times, logging each
        //! run's timings tagged \a tag.
        function timeRootToggles(view, tag) {
            for (let run = 0; run < toggleRuns; ++run) {
                const start = Date.now()
                view.collapse(0)
                view.forceLayout()
                const collapsed = Date.now()
                view.expand(0)
                view.forceLayout()
                const expanded = Date.now()
                console.info(tag, "root collapse ms", collapsed - start, "expand ms", expanded - collapsed,
                             "rows", view.rows)
            }
        }

        function addUnique(list, value) {
            if (list.indexOf(value) < 0) {
                list.push(value)
            }
        }

        // Scrolling across the meeting of a node's own fix rows and its
        // attached ones, and collapsing and expanding a cave above them,
        // hands each fix delegate rows of one kind only, and
        // the lock, the right-click menu and the pick button a delegate built
        // stay the objects it built. The timings and object counts are
        // logged for comparison; only the kinds and identities are asserted.
        function test_aScrolledFixDelegateKeepsOneKindAndItsControls() {
            const fixRows = makeMixedFixCaves("fix-station-delegate-kinds")
            const view = openFixStationTree("Source/Data/Fix Stations")
            // The root, every cave's node row and every fix.
            tryVerify(() => view.rows >= 1 + mixedCaveCount + fixRows, 10000, "every node and fix has a row")
            waitForRendering(rootId)
            verify(view.contentHeight > 2 * view.height,
                   "the tree is tall enough to scroll, content " + view.contentHeight
                   + " view " + view.height + "; raise mixedCaveCount")
            const rowCount = view.rows

            timeRootToggles(view, "SP2")
            tryCompare(view, "rows", rowCount, 5000, "expanding the root shows every row again")

            const scrollStart = Date.now()
            const timedSteps = sweep(view, () => {})
            console.info("SP2 scroll top-bottom-top ms", Date.now() - scrollStart, "steps", timedSteps,
                         "rows", rowCount)

            // One record per fix delegate: the stations and kinds it showed
            // and the lock, menu and pick button objects found under it.
            const records = []
            const recordOf = (item) => {
                const known = records.find(record => record.item === item)
                if (known !== undefined) {
                    return known
                }
                const record = { item: item, stations: [], kinds: [], controls: [[], [], []] }
                records.push(record)
                return record
            }
            const shownFixItems = () => {
                const items = []
                for (let row = 0; row < view.rows; ++row) {
                    const item = view.itemAtCell(Qt.point(0, row))
                    if (item !== null && String(item.objectName).startsWith("fixRow.")) {
                        items.push(item)
                    }
                }
                return items
            }
            const kindOf = (item) => String(item.stationName).startsWith(mixedOwnPrefix) ? "editable" : "readOnly"
            const visit = () => {
                for (const item of shownFixItems()) {
                    const record = recordOf(item)
                    addUnique(record.stations, String(item.stationName))
                    addUnique(record.kinds, kindOf(item))
                    for (let p = 0; p < fixControlPrefixes.length; ++p) {
                        const control = rootId.findChildWithPrefix(item, fixControlPrefixes[p])
                        if (control !== null) {
                            addUnique(record.controls[p], control)
                        }
                    }
                }
            }
            visit()
            const trackedSteps = sweep(view, visit)

            // Collapsing a cave mid-tree shifts every row below it, and the
            // pool hands each shifted row the delegate last used at that
            // table row, which can be a delegate of the other fix kind.
            const nodeRowOf = (name) => {
                for (let row = 0; row < view.rows; ++row) {
                    const item = view.itemAtCell(Qt.point(0, row))
                    if (item !== null && item.objectName === "nodeRow." + name) {
                        return row
                    }
                }
                return -1
            }
            for (let c = 0; c < toggledCaveCount; ++c) {
                const name = "M" + c
                const nodeRow = nodeRowOf(name)
                verify(nodeRow >= 0, name + " has a row on screen")
                const start = Date.now()
                view.collapse(nodeRow)
                view.forceLayout()
                visit()
                view.expand(nodeRow)
                view.forceLayout()
                visit()
                console.info("SP2 node", name, "collapse+expand ms", Date.now() - start)
            }
            tryCompare(view, "rows", rowCount, 5000, "expanding the caves shows every row again")

            const reused = records.filter(record => record.stations.length > 1)
            const mixed = records.filter(record => record.kinds.length > 1)
            const rebuilt = records.filter(record => record.controls.some(objects => objects.length > 1))
            const editable = records.filter(record => record.kinds.indexOf("editable") >= 0)
            const readOnly = records.filter(record => record.kinds.indexOf("readOnly") >= 0)
            const distinct = fixControlPrefixes.map((prefix, p) =>
                records.reduce((sum, record) => sum + record.controls[p].length, 0))
            console.info("SP2 sweep steps", trackedSteps, "fix delegates", records.length,
                         "reused", reused.length, "showing both kinds", mixed.length,
                         "rebuilding a control", rebuilt.length,
                         "editable delegates", editable.length, "read-only delegates", readOnly.length)
            console.info("SP2 distinct objects seen: locks", distinct[0], "menus", distinct[1],
                         "pick buttons", distinct[2])

            for (const kind of ["editable", "readOnly"]) {
                const item = shownFixItems().find(shown => kindOf(shown) === kind)
                if (item !== undefined) {
                    console.info("SP2 one", kind, "fix delegate: loaders",
                                 rootId.countDescendants(item, child => child instanceof Loader),
                                 "items", rootId.countDescendants(item, () => true))
                }
            }

            verify(reused.length > 0, "the sweep hands some fix delegate a second row")
            compare(mixed.length, 0, "every fix delegate shows rows of one kind only")
            compare(rebuilt.length, 0, "every fix delegate keeps the lock, menu and pick button it built")
        }

        // The lock's tooltip names the file in the tooltip colors in the
        // light scheme.
        function test_readOnlyLockToolTipFollowsTheLightTheme() {
            const cave = makeSavedCaveAttach("fix-station-lock-tooltip",
                                             "external-centerlines/survex_blocks.svx")
            tryVerify(() => subtreeAttachedFixCount(cave) === 1, 20000, "the scan lists the file's fix")
            openFixStationsOf(cave)
            rootId.verifyLightToolTip(this, waitForChild("readOnlyLock.0"), "survex_blocks.svx")
        }

        // The bare-fix badge's tooltip names the file in the tooltip colors
        // in the light scheme.
        function test_bareFixBadgeToolTipFollowsTheLightTheme() {
            saveProjectAs("fix-station-bare-fix-tooltip")

            // A native fix gives the project its frame, so the badge shows.
            RootData.region.addCave()
            const anchorCave = RootData.region.cave(RootData.region.caveCount - 1)
            anchorCave.name = "AnchorCave"
            const anchors = anchorCave.fixStations
            compare(anchors.addFixStation("A1"), 0)
            anchors.setData(anchors.index(0), "EPSG:32648", FixStationModel.InputCSRole)
            anchors.setData(anchors.index(0), 500000.0, FixStationModel.EastingRole)
            anchors.setData(anchors.index(0), 3000000.0, FixStationModel.NorthingRole)
            anchors.setData(anchors.index(0), 100.0, FixStationModel.ElevationRole)
            tryVerify(() => RootData.region.geoReference.hasCoordinateSystem, 5000,
                      "the anchoring fix gives the project a frame")

            const cave = makeAttachedCave("AttachedCave", TestHelper.testcasesDatasetSourcePath(
                                              "external-centerlines/survex_bare_fix/bare_fix.svx"))
            tryVerify(() => subtreeAttachedFixCount(cave) === 1, 20000, "the scan lists the file's fix")
            openFixStationsOf(cave)
            rootId.verifyLightToolTip(this, waitForChild("noCoordinateSystemBadge.0"), "bare_fix.svx")
        }

        // A .mak naming a UTM zone and no datum reads in Compass's default
        // datum, and the cave page counts its fix.
        function test_aZoneWithoutADatumReadsInCompassDefault() {
            saveProjectAs("fix-station-default-datum")
            const cave = makeAttachedCave("AttachedCave", TestHelper.testcasesDatasetSourcePath(
                                              "external-centerlines/compass_zone_no_datum.mak"))
            tryVerify(() => subtreeAttachedFixCount(cave) === 1, 20000, "the scan lists the file's fix")

            RootData.pageSelectionModel.currentPageAddress =
                "Source/Data/Node=" + String(cave.name) + "/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should land on fixStationPage")
            tryCompare(findChild(RootData.pageView.currentPageItem, "fixStationTreeView"), "count", 1)
            compare(waitForChild("stationCell.0").text, "compass_zone_no_datum.A1")
            // The System column names the system and the code cavern places it in.
            compare(waitForChild("inputCSText.0").text, "North American 1927, UTM zone 13N (EPSG:26713)")
            // The .mak writes [f,1568241.5,14534120.7,5429.8]; the row shows
            // the meters cavern solves with, in the project's units.
            compare(waitForChild("coordinateCell.0").text, "478000.0092, 4429999.98936, 1655.003m")
            verify(waitForChild("readOnlyLock.0") !== null, "the row is the file's")

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Node=" + String(cave.name)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      5000, "should land on cavePage")
            tryCompare(waitForChild("fixStationsLink"), "text", "1")
        }

        // A file fix with no *cs is read in the system the file's *cs out
        // names, and its row says so, naming the file and line. The read-in
        // holds once houping's own-system fix gives the project its frame.
        function test_aFixWithNoSystemSaysWhereItIsReadIn() {
            saveProjectAs("fix-station-read-in")
            const cave = makeAttachedCave("AttachedCave", TestHelper.testcasesDatasetSourcePath(
                                              "external-centerlines/survex_bare_fix_tree/north/north_cs_out.svx"))
            tryVerify(() => subtreeAttachedFixCount(cave) === 2, 20000, "the scan lists both fixes")
            tryVerify(() => RootData.region.geoReference.hasCoordinateSystem, 20000,
                      "houping's fix gives the project a frame")
            tryCompare(RootData.region.localProjection, "anchorDescription", "houping.h1 — AttachedCave")

            RootData.pageSelectionModel.currentPageAddress =
                "Source/Data/Node=" + String(cave.name) + "/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should land on fixStationPage")
            tryCompare(findChild(RootData.pageView.currentPageItem, "fixStationTreeView"), "count", 2)

            // houping names its *cs; sanqiao's entrance names none.
            compare(waitForChild("stationCell.0").text, "houping.h1")
            compare(liveChild("coordinateWarning.0").message, "")
            compare(waitForChild("stationCell.1").text, "sanqiao.e1")
            tryCompare(waitForChild("coordinateWarning.1"), "message",
                       "Read in UTM48N, the system the file's *cs out names: "
                       + "sanqiao/fixedpts/ents.svx line 3 names no coordinate system for this fix.")
        }

        //! Opens the Fix Stations page of \a node and returns the page.
        function openFixStationsOf(node) {
            RootData.pageSelectionModel.currentPageAddress =
                "Source/Data/Node=" + String(node.name) + "/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should land on fixStationPage")
            return RootData.pageView.currentPageItem
        }

        // A *fix the file marks "reference" shows the coordinate alone and a
        // chip saying so; a plain fix carries no chip.
        function test_referenceChipOnAttachedRow() {
            saveProjectAs("fix-station-reference-chip")
            const cave = makeAttachedCave("AttachedCave", TestHelper.testcasesDatasetSourcePath(
                                              "external-centerlines/survex_reference_fix/reference_fix.svx"))
            tryVerify(() => subtreeAttachedFixCount(cave) === 2, 20000, "the scan lists both fixes")

            const page = openFixStationsOf(cave)
            tryCompare(findChild(page, "fixStationTreeView"), "count", 2)
            compare(waitForChild("stationCell.0").text, "ref.A")
            verify(waitForChild("referenceChip.0") !== null, "the reference fix carries the chip")
            compare(waitForChild("coordinateCell.0").text, "10, 20, 30.000m")

            compare(waitForChild("stationCell.1").text, "ref.B")
            compare(liveChild("referenceChip.1"), null, "the plain fix carries none")
        }

        // A file fix in a grid shows its coordinate in the project's units,
        // the way a native row does, and its editor offers the text as the
        // file writes it.
        function test_anAttachedRowShowsItsCoordinateInTheProjectUnits() {
            saveProjectAs("fix-station-attached-units")
            const cave = makeAttachedCave("AttachedCave", TestHelper.testcasesDatasetSourcePath(
                                              "external-centerlines/survex_native_cs_scope/own_cs_fix.svx"))
            tryVerify(() => subtreeAttachedFixCount(cave) === 1, 20000, "the scan lists the file's fix")

            const page = openFixStationsOf(cave)
            tryCompare(findChild(page, "fixStationTreeView"), "count", 1)
            compare(waitForChild("stationCell.0").text, "scoped.S1")
            const coordinateCell = waitForChild("coordinateCell.0")
            const previousUnits = RootData.region.unitSystem
            RootData.region.unitSystem = Units.Metric
            tryCompare(coordinateCell, "text", "500200, 3000000, 100.000m", 5000,
                       "the file's meters read with the project's unit")

            RootData.region.unitSystem = Units.Imperial
            tryCompare(coordinateCell, "text", "500200, 3000000, 328.08ft", 5000,
                       "the elevation follows the project's unit system, suffix and all")
            compare(coordinateCell.editValue, "500200 3000000 100")

            RootData.region.unitSystem = previousUnits
        }

        // In a georeferenced project, a file fix with no coordinate system
        // says so on its row, and the attachment owner's page picks the system
        // the solve reads it in.
        function test_bareFixControlAndBadge() {
            saveProjectAs("fix-station-bare-fix")

            // A native fix in UTM zone 48N gives the project its frame.
            RootData.region.addCave()
            const anchorCave = RootData.region.cave(RootData.region.caveCount - 1)
            anchorCave.name = "AnchorCave"
            const anchors = anchorCave.fixStations
            compare(anchors.addFixStation("A1"), 0)
            anchors.setData(anchors.index(0), "EPSG:32648", FixStationModel.InputCSRole)
            anchors.setData(anchors.index(0), 500000.0, FixStationModel.EastingRole)
            anchors.setData(anchors.index(0), 3000000.0, FixStationModel.NorthingRole)
            anchors.setData(anchors.index(0), 100.0, FixStationModel.ElevationRole)
            tryVerify(() => RootData.region.geoReference.hasCoordinateSystem, 5000,
                      "the anchoring fix gives the project a frame")

            const cave = makeAttachedCave("AttachedCave", TestHelper.testcasesDatasetSourcePath(
                                              "external-centerlines/survex_bare_fix/bare_fix.svx"))
            tryVerify(() => subtreeAttachedFixCount(cave) === 1, 20000, "the scan lists the file's fix")

            const page = openFixStationsOf(cave)
            tryCompare(findChild(page, "fixStationTreeView"), "count", 1)
            compare(waitForChild("stationCell.0").text, "bare.A")

            const ownerRow = waitForChild("nodeRow.AttachedCave")
            compare(rootId.countWithPrefix(ownerRow, "bareFixSystemCombo."), 0,
                    "the owner's row builds no picker at rest")
            compare(waitForChild("bareFixSystemValue.AttachedCave").text, "unset",
                    "the owner's page names the unset system for the file's bare fixes")
            const badge = waitForChild("noCoordinateSystemBadge.0")

            const combo = openOwnerPicker("AttachedCave")
            combo.committed("UTM48N")
            tryVerify(() => cave.externalCenterline.fixCoordinateSystem === "UTM48N", 5000,
                      "the choice lands on the attachment")
            tryVerify(() => badge.text.indexOf("UTM48N") >= 0, 5000,
                      "the badge names the chosen system")
        }

        // In a project with no frame, a file fix with no coordinate system is
        // placed at the project origin, and its row says so.
        function test_aBareFixWithNoFrameSaysItIsPlacedAtTheOrigin() {
            saveProjectAs("fix-station-bare-fix-origin")
            const cave = makeAttachedCave("AttachedCave", TestHelper.testcasesDatasetSourcePath(
                                              "external-centerlines/survex_bare_fix/bare_fix.svx"))
            tryVerify(() => subtreeAttachedFixCount(cave) === 1, 20000, "the scan lists the file's fix")
            tryCompare(RootData.region.geoReference, "state", GeoReference.LocalGrid, 10000)

            const page = openFixStationsOf(cave)
            tryCompare(findChild(page, "fixStationTreeView"), "count", 1)
            tryVerify(() => {
                const badge = liveChild("noCoordinateSystemBadge.0")
                return badge !== null && badge.visible && badge.text.indexOf("origin") >= 0
            }, 10000, "the badge says the fix is placed at the origin")
        }

        // In a project with no frame, the system chosen for a file's bare fix
        // gives the project its frame, and the Data page names the fix it is
        // centered on.
        function test_aChosenBareFixSystemGivesTheProjectAFrame() {
            saveProjectAs("fix-station-bare-fix-frame")
            const cave = makeAttachedCave("AttachedCave", TestHelper.testcasesDatasetSourcePath(
                                              "external-centerlines/survex_bare_fix/bare_fix.svx"))
            tryVerify(() => subtreeAttachedFixCount(cave) === 1, 20000, "the scan lists the file's fix")
            compare(RootData.region.geoReference.hasCoordinateSystem, false)

            const page = openFixStationsOf(cave)
            tryCompare(findChild(page, "fixStationTreeView"), "count", 1)
            verify(waitForChild("bareFixSystemValue.AttachedCave") !== null)

            page.setBareFixCoordinateSystem(0, "UTM48N")
            tryVerify(() => cave.externalCenterline.fixCoordinateSystem === "UTM48N", 5000,
                      "the choice lands on the attachment")
            tryVerify(() => RootData.region.geoReference.hasCoordinateSystem, 10000,
                      "the choice gives the project a frame")
            tryVerify(() => {
                const badge = liveChild("noCoordinateSystemBadge.0")
                return badge !== null && badge.visible && badge.text.indexOf("UTM48N") >= 0
            }, 10000, "the badge names the chosen system")
            tryVerify(() => RootData.region.localProjection.anchorDescription.indexOf("bare.A") >= 0,
                      5000, "the Data page names the fix the frame is centered on")
        }

        // Once a file's bare fix has a system, the owner's page reads its grid
        // convergence at that fix.
        function test_aChosenBareFixSystemGivesTheNodePageAConvergence() {
            saveProjectAs("fix-station-bare-fix-convergence")
            const cave = makeAttachedCave("AttachedCave", TestHelper.testcasesDatasetSourcePath(
                                              "external-centerlines/survex_bare_fix/bare_fix.svx"))
            tryVerify(() => subtreeAttachedFixCount(cave) === 1, 20000, "the scan lists the file's fix")

            const page = openFixStationsOf(cave)
            tryCompare(findChild(page, "fixStationTreeView"), "count", 1)
            verify(waitForChild("bareFixSystemValue.AttachedCave") !== null)
            page.setBareFixCoordinateSystem(0, "UTM48N")
            tryVerify(() => RootData.region.geoReference.hasCoordinateSystem, 10000,
                      "the choice gives the project a frame")

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Node=" + String(cave.name)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      5000, "should land on cavePage")
            const readout = findChild(RootData.pageView.currentPageItem, "gridConvergenceValue")
            verify(readout !== null, "the node page shows the grid convergence")
            tryVerify(() => readout.text.indexOf("bare.A") >= 0, 10000,
                      "the readout converges at the file's fix: " + readout.text)
        }

        // The page as a tree over the node and its subtree: a row per node, the
        // node's fixes under it, attached fixes under the node or derived trip
        // whose file holds them. survex_subtree_fixes/ridge.svx fixes R0 itself
        // and includes entrances (E1 reference, E2), deep/lower (L1 under *cs
        // UTM16N) and deep/empty (no fix), each one leaf *begin block and so
        // one derived trip.

        //! Attaches the subtree fixture under the region's root and waits for
        //! the scan to list all of its fixes.
        function attachSubtreeFixture() {
            const root = attachSourceToRegion(TestHelper.testcasesDatasetSourcePath(subtreeFixture))
            tryVerify(() => subtreeAttachedFixCount(root) === subtreeFixCount, 20000,
                      "the scan lists every fix in the file tree")
            return root
        }

        //! A native fix in UTM zone 48N on a new cave named \a caveName, which
        //! gives the project its frame.
        function addAnchorCave(caveName) {
            RootData.region.addCave()
            const anchorCave = RootData.region.cave(RootData.region.caveCount - 1)
            anchorCave.name = caveName
            const anchors = anchorCave.fixStations
            compare(anchors.addFixStation("A1"), 0)
            anchors.setData(anchors.index(0), "EPSG:32648", FixStationModel.InputCSRole)
            anchors.setData(anchors.index(0), 500000.0, FixStationModel.EastingRole)
            anchors.setData(anchors.index(0), 3000000.0, FixStationModel.NorthingRole)
            anchors.setData(anchors.index(0), 100.0, FixStationModel.ElevationRole)
            return anchorCave
        }

        function test_treeShowsSubtreeFixes() {
            saveProjectAs("fix-station-tree")
            const root = attachSubtreeFixture()
            compare(root.name, "ridge")

            const page = openFixStationsOf(root)
            verify(waitForChild("fixStationTreeView") !== null, "the page is a tree")

            const rootRow = waitForChild("nodeRow.ridge")
            for (const tripName of ["entrances", "lower"]) {
                verify(waitForChild("tripRow." + tripName) !== null, tripName + " has a row")
            }
            for (const station of ["R0", "entrances.E1", "entrances.E2", "lower.L1"]) {
                verify(waitForChild("fixRow." + station) !== null, station + " has a row")
            }
            compare(liveChild("tripRow.empty"), null, "a trip that fixes nothing has no row")
            compare(liveChild("nodeRow.entrances"), null, "a leaf block is no node row")
            tryVerify(() => rootId.showsText(rootRow, "1 fix"), 5000,
                      "the owner's row counts its own fix")
        }

        // A derived node's name links to that node's Fix Stations page.
        // survex_block_rows/mixed.svx's block cave nests the leaf inner, so
        // cave is a derived node.
        function test_aDerivedNodeNameOpensItsFixStationsPage() {
            saveProjectAs("fix-station-tree-name-link")
            const root = attachSourceToRegion(TestHelper.testcasesDatasetSourcePath(
                                                  "external-centerlines/survex_block_rows/mixed.svx"))
            tryVerify(() => subtreeAttachedFixCount(root) === 2, 20000,
                      "the scan lists every fix in the file tree")
            openFixStationsOf(root)

            mouseClick(waitForChild("nodeNameLink.cave"))
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage"
                            && RootData.pageView.currentPageItem.cave !== null
                            && RootData.pageView.currentPageItem.cave.name === "cave",
                      5000, "the link opens cave's Fix Stations page")
            verify(RootData.pageSelectionModel.currentPageAddress.endsWith("Node=cave/Fix Stations"),
                   "the address names cave's Fix Stations page, got "
                   + RootData.pageSelectionModel.currentPageAddress)
        }

        function test_collapseHidesLeaves() {
            saveProjectAs("fix-station-tree-collapse")
            const root = attachSubtreeFixture()
            openFixStationsOf(root)

            verify(waitForChild("fixRow.entrances.E1") !== null)
            const chevron = waitForChild("tripChevron.entrances")
            mouseClick(chevron)
            tryVerify(() => {
                const row = liveChild("fixRow.entrances.E1")
                return row === null || !row.visible
            }, 5000, "collapsing entrances hides its fixes")

            mouseClick(waitForChild("tripChevron.entrances"))
            verify(waitForChild("fixRow.entrances.E1") !== null, "expanding shows them again")
        }

        function test_ownerControlUpdatesDescendants() {
            saveProjectAs("fix-station-tree-owner-control")
            addAnchorCave("AnchorCave")
            tryVerify(() => RootData.region.geoReference.hasCoordinateSystem, 5000,
                      "the anchoring fix gives the project a frame")

            const root = attachSubtreeFixture()
            const page = openFixStationsOf(root)

            // The owner's control sits on the owner's node row.
            const ownerRow = waitForChild("nodeRow.ridge")
            tryVerify(() => findChild(ownerRow, "bareFixSystemValue.ridge") !== null, 5000,
                      "the owner's row names a system for the file's bare fixes")
            verify(isInside(openOwnerPicker("ridge"), ownerRow),
                   "and opens its picker there on a double-click")

            const entrancesRow = waitForChild("tripRow.entrances")
            tryVerify(() => rootId.showsText(entrancesRow, "without coordinate system"), 5000,
                      "the descendant's badge carries the unset system")
            const unsetInherited = liveChild("inheritedSystem.entrances")
            verify(unsetInherited === null || !unsetInherited.visible,
                   "an unset subtree shows no inherited-system row")

            // The page's own node is attachmentOwners[0].
            page.setBareFixCoordinateSystem(0, "UTM48N")
            tryVerify(() => root.externalCenterline.fixCoordinateSystem === "UTM48N", 5000,
                      "the choice lands on the attachment")

            const e2Row = waitForChild("fixRow.entrances.E2")
            tryVerify(() => {
                const badge = rootId.findChildWithPrefix(e2Row, "noCoordinateSystemBadge")
                return badge !== null && badge.text.indexOf("UTM48N") >= 0
            }, 5000, "the descendant's bare fix reads in the owner's choice")

            const inherited = waitForChild("inheritedSystem.entrances")
            tryVerify(() => inherited.text.indexOf("UTM48N") >= 0
                            && inherited.text.indexOf("ridge") >= 0, 5000,
                      "the descendant names the system and the node it is set on")
        }

        function test_rootPageListsEveryFix() {
            saveProjectAs("fix-station-tree-root-page")
            const regionRoot = RootData.region.rootNode
            RootData.region.addCave()
            const nativeCave = RootData.region.cave(RootData.region.caveCount - 1)
            nativeCave.name = "Pit"
            compare(nativeCave.fixStations.addFixStation("p1"), 0)
            attachSubtreeFixture()

            // The region's root node's page is the Data page, so its Fix
            // Stations page is the Data page's child.
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "the region root has a Fix Stations page")
            compare(RootData.pageView.currentPageItem.cave, regionRoot)

            verify(waitForChild("fixRow.R0") !== null, "the attached root's fix is listed")
            verify(waitForChild("fixRow.p1") !== null, "a native cave's fix is listed")
        }

        // survex_block_rows/leaf.svx is one leaf block, simmons_hall, so the
        // block is a trip, and its bare *fix st1 sits under the trip's row.
        function test_aTripRowHoldsItsBlocksFixesAndInheritsTheOwnersSystem() {
            saveProjectAs("fix-station-tree-trip-row")
            addAnchorCave("AnchorCave")
            tryVerify(() => RootData.region.geoReference.hasCoordinateSystem, 5000,
                      "the anchoring fix gives the project a frame")

            const root = attachSourceToRegion(TestHelper.testcasesDatasetSourcePath(
                                                  "external-centerlines/survex_block_rows/leaf.svx"))
            compare(root.name, "leaf")
            const page = openFixStationsOf(root)

            const tripRow = waitForChild("tripRow.simmons_hall")
            verify(waitForChild("fixRow.simmons_hall.st1") !== null, "the trip's fix has a row")
            compare(liveChild("nodeRow.simmons_hall"), null, "the block is no node row")
            tryVerify(() => rootId.showsText(tripRow, "1 without coordinate system"), 5000,
                      "the trip row carries the block's badge")
            compare(findChild(tripRow, "bareFixSystemCombo.simmons_hall"), null,
                    "the trip row offers no system of its own")
            compare(findChild(tripRow, "bareFixSystemValue.simmons_hall"), null,
                    "nor names one")

            const unsetInherited = liveChild("inheritedSystem.simmons_hall")
            verify(unsetInherited === null || !unsetInherited.visible,
                   "an unset subtree shows no inherited-system row")

            // The owner's control stays on the owner's node row.
            const ownerRow = waitForChild("nodeRow.leaf")
            tryVerify(() => findChild(ownerRow, "bareFixSystemValue.leaf") !== null, 5000,
                      "the owner's row names a system for the file's bare fixes")
            verify(isInside(openOwnerPicker("leaf"), ownerRow),
                   "and opens its picker there on a double-click")
            page.setBareFixCoordinateSystem(0, "UTM48N")
            tryVerify(() => root.externalCenterline.fixCoordinateSystem === "UTM48N", 5000,
                      "the choice lands on the attachment")

            const inherited = waitForChild("inheritedSystem.simmons_hall")
            verify(isInside(inherited, liveChild("tripRow.simmons_hall")),
                   "the inherited system shows on the trip row")
            tryVerify(() => inherited.text.indexOf("UTM48N") >= 0
                            && inherited.text.indexOf("leaf") >= 0, 5000,
                      "the trip row names the system and the node it is set on")

            mouseClick(waitForChild("tripChevron.simmons_hall"))
            tryVerify(() => {
                const row = liveChild("fixRow.simmons_hall.st1")
                return row === null || !row.visible
            }, 5000, "collapsing the trip hides its fixes")
            mouseClick(waitForChild("tripChevron.simmons_hall"))
            verify(waitForChild("fixRow.simmons_hall.st1") !== null, "expanding shows them again")

            const toggle = findChild(page, "expandToggleButton")
            verify(toggle !== null, "the page has one expand/collapse toggle")
            tryVerify(() => toggle.visible, 5000, "a page whose rows are trip rows offers Collapse all")
        }

        //! Attaches bare_fix.svx to trips t3 and t17 of \a cave and waits for
        //! the scan to list each one's bare fix on the cave.
        function attachBareFixesToTwoTrips(cave) {
            const source = TestHelper.testcasesDatasetPath(
                             "external-centerlines/survex_bare_fix/bare_fix.svx")
            attachSourceToTrip(cave.trip(3), source)
            attachSourceToTrip(cave.trip(17), source)
            tryVerify(() => cave.attachedFixes.bareFixCount(cave.trip(3).id) > 0
                            && cave.attachedFixes.bareFixCount(cave.trip(17).id) > 0,
                      20000, "the scan lists both trips' bare fixes on the cave")
        }

        //! Logs the owner labels and pickers under \a item, tagged \a what.
        function logOwnerControls(what, item) {
            console.info("SP1", what,
                         "bareFixSystemValue", rootId.countWithPrefix(item, "bareFixSystemValue."),
                         "bareFixSystemCombo", rootId.countWithPrefix(item, "bareFixSystemCombo."),
                         "CSComboBox", rootId.countCSComboBoxes(item))
        }

        // A node row names the system of each trip whose attachment holds a
        // fix with no coordinate system, and builds a picker only for the one
        // the user double-clicks: a cave of many trips builds nothing for the
        // trips that need none.
        function test_aNodeRowBuildsOwnerControlsOnlyForTripsWithBareFixes() {
            saveProjectAs("fix-station-owner-controls")
            const big = makeManyTripCave("Big", 40)
            attachBareFixesToTwoTrips(big)

            openFixStationsOf(big)
            const row = waitForChild("nodeRow.Big")
            waitForRendering(rootId)
            logOwnerControls("Big row at rest", row)
            compare(rootId.countWithPrefix(row, "bareFixSystemCombo."), 0,
                    "no picker at rest")
            compare(rootId.countCSComboBoxes(row), 0, "no CSComboBox at rest")
            waitForChild("bareFixSystemValue.t3")
            waitForChild("bareFixSystemValue.t17")
            compare(rootId.countWithPrefix(row, "bareFixSystemValue."), 2,
                    "one owner label per trip whose file holds a bare fix")

            // One picker is open at a time.
            openOwnerPicker("t17")
            const combo = openOwnerPicker("t3")
            logOwnerControls("Big row with t3's picker open", row)
            compare(rootId.countWithPrefix(row, "bareFixSystemCombo."), 1,
                    "opening t3's picker closes t17's")
            compare(rootId.countCSComboBoxes(row), 1)
            compare(liveChild("bareFixSystemCombo.t17"), null)

            combo.committed("UTM48N")
            tryVerify(() => big.trip(3).externalCenterline.fixCoordinateSystem === "UTM48N", 5000,
                      "the choice lands on t3's attachment")
            compare(big.trip(17).externalCenterline.fixCoordinateSystem, "",
                    "and on no other trip")
            RootData.futureManagerModel.waitForFinished()
            compare(liveChild("bareFixSystemCombo.t3"), combo,
                    "the picker stays open across the rescan its choice starts")

            keyClick(Qt.Key_Escape)
            tryVerify(() => rootId.countWithPrefix(row, "bareFixSystemCombo.") === 0, 5000,
                      "Escape closes the picker")
            tryCompare(waitForChild("bareFixSystemValue.t3"), "text", CSFormat.displayName("UTM48N"),
                       5000, "and the label names the chosen system")
            compare(waitForChild("bareFixSystemValue.t17").text, "unset")
        }

        // A node none of whose trips' files holds a bare fix builds no owner
        // control at all.
        function test_aNodeRowWithNoBareFixOwnersBuildsNoOwnerControls() {
            const plain = makeManyTripCave("Plain", 40)
            openFixStationsOf(plain)
            const row = waitForChild("nodeRow.Plain")
            logOwnerControls("Plain row", row)
            compare(rootId.countWithPrefix(row, "bareFixSystemValue."), 0, "no owner label")
            compare(rootId.countWithPrefix(row, "bareFixSystemCombo."), 0, "no owner picker")
            compare(rootId.countCSComboBoxes(row), 0, "no CSComboBox")
        }

        // Collapsing and expanding the region's root and scrolling the whole
        // tree build no system picker on any node row; only the double-click
        // does. The timings are logged for comparison, never asserted.
        function test_aRootCollapseAndExpandBuildsNoSystemPickers() {
            saveProjectAs("fix-station-owner-controls-toggle")
            const big = makeManyTripCave("Big", 40)
            attachBareFixesToTwoTrips(big)
            const caveCount = 40
            for (let c = 1; c <= caveCount; ++c) {
                makeManyTripCave("C" + c, 20 + (c * 7) % 30)
            }

            const view = openFixStationTree("Source/Data/Fix Stations")
            // The root, Big, Big's two attached fixes and every cave.
            tryCompare(view, "rows", caveCount + 4, 10000, "every node and fix has a row")
            waitForChild("nodeRow.Big")
            waitForRendering(rootId)
            logOwnerControls("tree at open", view.contentItem)
            const pickersAtOpen = rootId.countWithPrefix(view.contentItem, "bareFixSystemCombo.")

            timeRootToggles(view, "SP1")

            const scrollStart = Date.now()
            const steps = sweep(view, () => {})
            console.info("SP1 scroll top-bottom-top ms", Date.now() - scrollStart, "steps", steps)

            logOwnerControls("tree after the toggles and the scroll", view.contentItem)
            compare(pickersAtOpen, 0, "the open tree builds no picker")
            compare(rootId.countWithPrefix(view.contentItem, "bareFixSystemCombo."), 0,
                    "the toggles and the scroll build no picker")
            compare(rootId.countWithPrefix(waitForChild("nodeRow.Big"), "bareFixSystemValue."), 2,
                    "Big's row still names its two owners' systems")
        }

        readonly property string popupCaveName: "Popups"
        readonly property string popupTripName: "t0"
        readonly property string popupUtmCS: "EPSG:32612"
        readonly property string popupOwnerCS: "UTM48N"
        //! An easting past UTM's domain, so the row's coordinate warns.
        readonly property double popupOutOfDomainEasting: 1478000.0
        readonly property double popupNorthing: 4430000.0

        //! \a root and every object under it reachable through an item's data,
        //! a popup's contentData and a Loader's item, each once.
        function objectsUnder(root) {
            const seen = new Set()
            const pending = [root]
            while (pending.length > 0) {
                const obj = pending.pop()
                if (obj === null || obj === undefined || seen.has(obj)) {
                    continue
                }
                seen.add(obj)
                const data = obj.data !== undefined ? obj.data
                                                    : (obj.contentData !== undefined ? obj.contentData : [])
                for (let i = 0; i < data.length; ++i) {
                    pending.push(data[i])
                }
                if (obj instanceof Loader && obj.item !== null) {
                    pending.push(obj.item)
                }
            }
            return Array.from(seen)
        }

        function popupsUnder(root) {
            return objectsUnder(root).filter(obj => obj instanceof T.Popup)
        }

        function layersUnder(root) {
            return objectsUnder(root).filter(obj => obj.layer !== undefined && obj.layer !== null
                                                    && obj.layer.enabled)
        }

        function namesOf(objects) {
            return objects.map(obj => String(obj)).join(" ")
        }

        //! How many of \a objects there are and which, read now: the layout
        //! switch destroys the wide rows' objects.
        function tally(what, objects) {
            return { what: what, count: objects.length, names: namesOf(objects) }
        }

        //! Logs \a row's object, item, popup and layer counts tagged \a what.
        function logRowCost(what, row) {
            console.info("SP3", what, row.objectName,
                         "objects", objectsUnder(row).length,
                         "items", rootId.countDescendants(row, () => true),
                         "popups", popupsUnder(row).length,
                         "layers", layersUnder(row).length)
        }

        //! Saves the project and builds popupCaveName with three fixes of its
        //! own and one trip that attaches bare_fix.svx: own0 names popupUtmCS,
        //! own1's coordinate leaves its system's domain, and the third has
        //! neither a name nor a system. Opens the cave's Fix Stations tree and returns it.
        function makePopupFixture(projectBaseName) {
            saveProjectAs(projectBaseName)
            const cave = RootData.region.addNode(null, SurveyNodeKind.Cave, popupCaveName)
            cave.addTrip()
            cave.trip(0).name = popupTripName
            const model = cave.fixStations
            const utmRow = model.addFixStation("own0")
            model.setData(model.index(utmRow), popupUtmCS, FixStationModel.InputCSRole)
            const outOfDomainRow = model.addFixStation("own1")
            model.setData(model.index(outOfDomainRow), "EPSG:32613", FixStationModel.InputCSRole)
            model.setData(model.index(outOfDomainRow), popupOutOfDomainEasting, FixStationModel.EastingRole)
            model.setData(model.index(outOfDomainRow), popupNorthing, FixStationModel.NorthingRole)
            model.addFixStation()
            model.setData(model.index(model.rowCount() - 1), "", FixStationModel.InputCSRole)
            attachSourceToTrip(cave.trip(0), TestHelper.testcasesDatasetPath(
                                   "external-centerlines/survex_bare_fix/bare_fix.svx"))
            tryVerify(() => subtreeAttachedFixCount(cave) > 0, 20000, "the scan lists the file's fix")

            const view = openFixStationTree("Source/Data/Node=" + popupCaveName + "/Fix Stations")
            tryVerify(() => liveChild("fixRow.own0") !== null && readOnlyFixRow(view) !== null,
                      10000, "the tree shows an editable and a read-only fix row")
            waitForRendering(rootId)
            return view
        }

        //! The first shown fix row that carries the read-only lock.
        function readOnlyFixRow(view) {
            for (let row = 0; row < view.rows; ++row) {
                const item = view.itemAtCell(Qt.point(0, row))
                if (item !== null && String(item.objectName).startsWith("fixRow.")
                        && rootId.countWithPrefix(item, "readOnlyLock.") > 0) {
                    return item
                }
            }
            return null
        }

        // A fix row at rest builds no tooltip, menu or offscreen layer: each
        // waits for the hover or right-click that shows it, in both layouts.
        // The per-row counts and the toggle and layout timings are logged
        // for comparison; only the counts are asserted.
        function test_aFixRowAtRestBuildsNoPopupOrLayer() {
            const view = makePopupFixture("fix-station-popups-at-rest")
            const wideEditable = liveChild("fixRow.own0")
            const wideReadOnly = readOnlyFixRow(view)
            const editableRow = wideEditable.fixRow
            logRowCost("wide editable", wideEditable)
            logRowCost("wide read-only", wideReadOnly)
            const counts = [
                tally("a wide editable row's popups", popupsUnder(wideEditable)),
                tally("a wide read-only row's popups", popupsUnder(wideReadOnly)),
                tally("a wide read-only row's layers", layersUnder(wideReadOnly)),
            ]

            timeRootToggles(view, "SP3")

            const wideWidth = rootId.width
            try {
                const start = Date.now()
                rootId.width = Theme.breakpointPanelCollapse / 2
                tryVerify(() => liveChild("narrowFixRow." + editableRow) !== null, 5000,
                          "the narrow layout shows the rows")
                console.info("SP3 first narrow layout build ms", Date.now() - start)
                waitForRendering(rootId)
                const narrowEditable = liveChild("fixRow.own0")
                // The narrow rows are taller, so the read-only one sits below the fold.
                view.contentY = Math.max(0, view.contentHeight - view.height)
                view.forceLayout()
                tryVerify(() => readOnlyFixRow(view) !== null, 5000, "the narrow layout shows the read-only row")
                const narrowReadOnly = readOnlyFixRow(view)
                logRowCost("narrow editable", narrowEditable)
                logRowCost("narrow read-only", narrowReadOnly)
                counts.push(tally("a narrow editable row's popups", popupsUnder(narrowEditable)))
                counts.push(tally("a narrow read-only row's popups", popupsUnder(narrowReadOnly)))
                counts.push(tally("a narrow read-only row's layers", layersUnder(narrowReadOnly)))
            } finally {
                rootId.width = wideWidth
            }
            tryVerify(() => liveChild("narrowFixRow." + editableRow) === null, 5000,
                      "the wide layout is back")

            for (const count of counts) {
                compare(count.count, 0, count.what + " at rest: " + count.names)
            }

            // The lock is tinted without an offscreen layer, in the subtle
            // text color it showed in before.
            const lock = waitForChild("readOnlyLock." + readOnlyFixRow(view).fixRow)
            const lockImage = objectsUnder(lock).find(obj => obj.source !== undefined
                                                         && String(obj.source).indexOf("lock.svg") >= 0)
            verify(lockImage !== undefined, "the lock shows lock.svg")
            verify(lockImage.color !== undefined, "the lock image " + lockImage + " carries its own color")
            verify(Qt.colorEqual(lockImage.color, Theme.textSubtle),
                   "the lock reads " + lockImage.color + ", Theme " + Theme.textSubtle)
        }

        // Hovering a fix row's system, warnings, lock and badge, and the
        // node row's bare-fix system, shows the window's one shared tooltip
        // with the text each showed before.
        function test_aFixRowsHoverShowsTheSharedToolTip() {
            const view = makePopupFixture("fix-station-popups-hover")
            const editable = liveChild("fixRow.own0")

            const csLabel = waitForChild("inputCSLabel." + editable.fixRow)
            let tip = rootId.hoverSharedToolTip(this, csLabel)
            verify(tip.text.indexOf(CSFormat.displayName(popupUtmCS)) >= 0,
                   "the tooltip names the full system: \"" + tip.text + "\"")
            rootId.leaveToolTip(this, tip)

            const outOfDomain = liveChild("fixRow.own1")
            const coordinateWarning = waitForChild("coordinateWarning." + outOfDomain.fixRow)
            tip = rootId.hoverSharedToolTip(this, coordinateWarning)
            compare(tip.text, coordinateWarning.message, "the coordinate warning says what is wrong")
            rootId.leaveToolTip(this, tip)

            const unnamed = liveChild("fixRow.")
            const noSystemLabel = waitForChild("inputCSLabel." + unnamed.fixRow)
            compare(noSystemLabel.text, "none")
            rootId.verifyNoSharedToolTip(this, noSystemLabel)

            const stationWarning = waitForChild("stationWarning." + unnamed.fixRow)
            tip = rootId.hoverSharedToolTip(this, stationWarning)
            compare(tip.text, stationWarning.message, "the station warning says what is wrong")
            rootId.leaveToolTip(this, tip)

            const readOnly = readOnlyFixRow(view)
            tip = rootId.hoverSharedToolTip(this, waitForChild("readOnlyLock." + readOnly.fixRow))
            verify(tip.text.indexOf("bare_fix.svx") >= 0, "the lock names the file: \"" + tip.text + "\"")
            rootId.leaveToolTip(this, tip)

            const badge = waitForChild("noCoordinateSystemBadge." + readOnly.fixRow)
            tip = rootId.hoverSharedToolTip(this, badge)
            verify(tip.text.indexOf(badge.unset ? "bare_fix.svx" : badge.readInWarning) >= 0,
                   "the badge says where the fix is placed: \"" + tip.text + "\"")
            rootId.leaveToolTip(this, tip)

            const ownerRow = waitForChild("nodeRow." + popupCaveName)
            compare(popupsUnder(ownerRow).length, 0,
                    "the owner's row builds no tooltip at rest: " + namesOf(popupsUnder(ownerRow)))
            const ownerLabel = waitForChild("bareFixSystemValue." + popupTripName)
            compare(ownerLabel.text, "unset")
            rootId.verifyNoSharedToolTip(this, ownerLabel)

            const combo = openOwnerPicker(popupTripName)
            combo.committed(popupOwnerCS)
            keyClick(Qt.Key_Escape)
            tryCompare(ownerLabel, "text", CSFormat.displayName(popupOwnerCS))
            tip = rootId.hoverSharedToolTip(this, ownerLabel)
            compare(tip.text, CSFormat.inlineDescription(popupOwnerCS),
                    "the owner's system names the full system")
            rootId.leaveToolTip(this, tip)
        }

        //! Right-clicks \a stationCell and checks that its row's menu, which
        //! nothing builds before, opens offering to remove \a stationName and
        //! to add a fix to \a nodeName, and hands that row to the remove
        //! question. Opens it a second time and checks that it opens where the
        //! first click opened it.
        function verifyRightClickMenu(stationCell, rowMenu, stationName, nodeName) {
            compare(rowMenu.menu, null, "the row builds no menu before a right-click")
            waitForRendering(rootId)
            const clickX = stationCell.width / 2
            const clickY = stationCell.height / 2
            mouseClick(stationCell, clickX, clickY, Qt.RightButton)
            const menu = rowMenu.menu
            verify(menu !== null, "the first right-click builds the menu")
            tryVerify(() => menu.opened, 5000, "and opens it")
            const menuAt = menu.parent.mapToItem(stationCell, menu.x, menu.y)
            fuzzyCompare(menuAt.x, clickX, 1, "the menu opens at the pointer")
            fuzzyCompare(menuAt.y, clickY, 1)
            compare(menu.count, 2, "the menu holds Remove and Add")
            compare(menu.itemAt(0).text, "Remove " + stationName)
            compare(menu.itemAt(1).text, "Add fix station to " + nodeName)
            const firstX = menu.x
            const firstY = menu.y

            const removeAskBox = findChild(RootData.pageView.currentPageItem, "removeChallange")
            mouseClick(menu.itemAt(0))
            tryVerify(() => !menu.visible, 5000, "choosing Remove closes the menu")
            tryVerify(() => removeAskBox.visible, 5000, "and asks whether to remove the fix")
            compare(removeAskBox.removeName, stationName)
            compare(removeAskBox.indexToRemove, rowMenu.row)
            mouseClick(findChild(removeAskBox, "cancelButton"))
            tryVerify(() => !removeAskBox.visible)

            mouseClick(stationCell, clickX, clickY, Qt.RightButton)
            compare(rowMenu.menu, menu, "the row keeps the menu it built")
            tryVerify(() => menu.opened, 5000, "a second right-click opens it again")
            compare(menu.x, firstX, "where the first click opened it")
            compare(menu.y, firstY)
            menu.close()
            tryVerify(() => !menu.visible)
        }

        // A fix row's right-click menu is built by the first right-click, and
        // that click opens it, in both layouts.
        function test_aFixRowsFirstRightClickBuildsAndOpensItsMenu() {
            makePopupFixture("fix-station-popups-menu")
            const fixRow = liveChild("fixRow.own0").fixRow
            verifyRightClickMenu(waitForChild("stationCell." + fixRow),
                                 liveChild("fixRowMenu." + fixRow), "own0", popupCaveName)

            const wideWidth = rootId.width
            try {
                rootId.width = Theme.breakpointPanelCollapse / 2
                tryVerify(() => liveChild("narrowFixRow." + fixRow) !== null, 5000,
                          "the narrow layout shows the rows")
                const narrowRow = liveChild("fixRow.own0")
                verifyRightClickMenu(waitForChild("stationCell." + narrowRow.fixRow),
                                     liveChild("fixRowMenu." + narrowRow.fixRow), "own0", popupCaveName)
            } finally {
                rootId.width = wideWidth
            }
        }

        // The pick button builds its own width-capped tooltip on the first
        // hover, and that tooltip still waits out its delay.
        function test_thePickButtonBuildsItsToolTipOnHover() {
            makePopupFixture("fix-station-popups-pick")
            const row = liveChild("fixRow.own0")
            const button = waitForChild("pickFromViewButton." + row.fixRow)
            verify(!button.canPick, "the project has no frame, so the tooltip holds the long sentence")
            compare(popupsUnder(button).length, 0,
                    "the pick button builds no tooltip before a hover: " + namesOf(popupsUnder(button)))

            waitForRendering(rootId)
            const hoverStart = Date.now()
            mouseMove(button, button.width / 2, button.height / 2)
            const tip = objectsUnder(button).find(obj => obj.objectName === "pickFromViewToolTip")
            verify(tip !== undefined, "hovering builds the button's tooltip: " + namesOf(popupsUnder(button)))
            verify(!tip.visible, "the tooltip waits for its delay")
            tryVerify(() => tip.visible, 5000, "then shows")
            const shownAfter = Date.now() - hoverStart
            console.info("SP3 pick tooltip shown after ms", shownAfter, "delay", button.tooltipDelay)
            verify(shownAfter >= button.tooltipDelay - 50,
                   "the tooltip showed after " + shownAfter + " ms, its delay is " + button.tooltipDelay)
            compare(tip.text, button.helpText)
            verify(tip.implicitWidth > button.tooltipWidth,
                   "the sentence is " + tip.implicitWidth + " wide, past the cap")
            compare(tip.width, button.tooltipWidth, "the cap wraps it")
            rootId.leaveToolTip(this, tip)
            compare(objectsUnder(button).find(obj => obj.objectName === "pickFromViewToolTip"), tip,
                    "the button keeps the tooltip it built, so it fades out like the others")
        }
    }
}
