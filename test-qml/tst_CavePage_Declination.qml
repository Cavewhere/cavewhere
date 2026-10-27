import QtQuick
import QtQml.Models
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    // Standalone instance — drives DeclinationSubmenu's list-based
    // multi-calibration path without depending on findChild order across the
    // cave-page row delegates.
    property list<TripCalibration> standaloneCalibrations: []

    DeclinationSubmenu {
        id: menuId
        tripCalibrations: rootId.standaloneCalibrations
    }

    TestCase {
        name: "CavePageDeclination"
        when: windowShown

        readonly property string autoItemName: "declinationAutoMenuItem"
        readonly property string manualItemName: "declinationManualMenuItem"

        property var cave: null

        function initTestCase() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.region.addCave()
            cave = RootData.region.cave(0)
            cave.name = "DeclCave"

            // Fix station so autoDeclination is "available" for every trip.
            cave.fixStations.addFixStation()
            const fixModel = cave.fixStations
            const idx = fixModel.index(0)
            fixModel.setData(idx, "a1", FixStationModel.StationNameRole)
            fixModel.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
            fixModel.setData(idx, 478000.0, FixStationModel.EastingRole)
            fixModel.setData(idx, 4430000.0, FixStationModel.NorthingRole)
            fixModel.setData(idx, 1655.0, FixStationModel.ElevationRole)

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Node=DeclCave"
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "cavePage")
        }

        function init() {
            cave.clearTrips()
            for (let i = 0; i < 5; ++i) {
                cave.addTrip()
                const trip = cave.trip(i)
                trip.name = "Trip-" + i
                trip.date = new Date(2024, 5, 1)
                trip.calibration.autoDeclination = false
            }
            const treeView = tree()
            tryCompare(treeView, "rowCount", 5, 5000,
                       "the cave's trips are the tree's rows")
            treeView.selectionModel.clearSelection()
            rootId.standaloneCalibrations = [cave.trip(0).calibration]
        }

        function cleanup() {
            rootId.standaloneCalibrations = []
        }

        function cavePage() {
            let p = RootData.pageView.currentPageItem
            verify(p !== null, "cave page must exist")
            return p
        }

        // The cave page's tree, rooted at the cave: its rows are the trips.
        function tree() {
            let treeView = findChild(cavePage(), "tripTree")
            verify(treeView !== null, "the cave page must show the survey tree")
            return treeView
        }

        // Picks exactly \a rows, the way a ctrl-click on each of them does.
        function selectRows(rows) {
            const treeView = tree()
            treeView.selectionModel.clearSelection()
            for (let i = 0; i < rows.length; ++i) {
                treeView.selectionModel.select(treeView.indexAtRow(rows[i]),
                                               ItemSelectionModel.Select
                                               | ItemSelectionModel.Rows)
            }
        }

        // What a calibration verb on the row \a row acts on.
        function calibrationsForRow(row) {
            const treeView = tree()
            return treeView.tripCalibrationsFor(treeView.objectAtRow(row))
        }

        // ── Submenu disables itself when nothing is wired ────────────────────

        function test_submenuDisabledWithoutCalibrations() {
            rootId.standaloneCalibrations = []

            // The DeclinationSubmenu IS the submenu now (no DataRightClickMouseMenu
            // wrapper), so check its enabled bit directly.
            tryVerify(() => !menuId.enabled, 500,
                      "submenu must be disabled when the list is empty")
        }

        // ── Multi-calibration: Auto/Manual flip every entry, untouched rows
        //    stay put. (The N=1 case is implicit — covered by setting a
        //    single-entry list below.) ────────────────────────────────────────

        function test_autoMenuItemAppliesToAllCalibrations() {
            rootId.standaloneCalibrations = [
                cave.trip(0).calibration,
                cave.trip(2).calibration,
                cave.trip(4).calibration
            ]

            findChild(menuId, autoItemName).triggered()

            compare(cave.trip(0).calibration.autoDeclination, true)
            compare(cave.trip(2).calibration.autoDeclination, true)
            compare(cave.trip(4).calibration.autoDeclination, true)
            compare(cave.trip(1).calibration.autoDeclination, false, "row 1 untouched")
            compare(cave.trip(3).calibration.autoDeclination, false, "row 3 untouched")
        }

        function test_manualMenuItemAppliesToAllCalibrations() {
            for (let i = 0; i < 5; ++i) {
                cave.trip(i).calibration.autoDeclination = true
            }
            rootId.standaloneCalibrations = [
                cave.trip(1).calibration,
                cave.trip(3).calibration
            ]

            findChild(menuId, manualItemName).triggered()

            compare(cave.trip(1).calibration.autoDeclination, false)
            compare(cave.trip(3).calibration.autoDeclination, false)
            compare(cave.trip(0).calibration.autoDeclination, true, "row 0 untouched")
            compare(cave.trip(2).calibration.autoDeclination, true, "row 2 untouched")
            compare(cave.trip(4).calibration.autoDeclination, true, "row 4 untouched")
        }

        // ── checked = every entry matches; mixed leaves both unchecked ───────

        function test_checkmarkReflectsHomogeneousState() {
            const autoItem = findChild(menuId, autoItemName)
            const manualItem = findChild(menuId, manualItemName)

            cave.trip(0).calibration.autoDeclination = false
            cave.trip(1).calibration.autoDeclination = false
            rootId.standaloneCalibrations = [
                cave.trip(0).calibration,
                cave.trip(1).calibration
            ]

            tryVerify(() => !autoItem.checked, 500)
            tryVerify(() => manualItem.checked, 500,
                      "both manual ⇒ Manual checked")

            cave.trip(0).calibration.autoDeclination = true
            cave.trip(1).calibration.autoDeclination = true
            tryVerify(() => autoItem.checked, 500,
                      "both auto ⇒ Auto checked")
            tryVerify(() => !manualItem.checked, 500)
        }

        function test_checkmarkReflectsMixedAsBothUnchecked() {
            const autoItem = findChild(menuId, autoItemName)
            const manualItem = findChild(menuId, manualItemName)

            cave.trip(0).calibration.autoDeclination = true
            cave.trip(1).calibration.autoDeclination = false
            rootId.standaloneCalibrations = [
                cave.trip(0).calibration,
                cave.trip(1).calibration
            ]

            tryVerify(() => !autoItem.checked, 500,
                      "mixed selection leaves Auto unchecked")
            tryVerify(() => !manualItem.checked, 500,
                      "mixed selection leaves Manual unchecked")
        }

        // ── CavePage row→selection-or-self resolution ────────────────────────

        function test_getCalibrationsForRowReturnsSingleRowWhenNotInSelection() {
            selectRows([2])

            const calibrations = calibrationsForRow(0)
            compare(calibrations.length, 1, "row 0 is not in selection ⇒ single-row scope")
            compare(calibrations[0], cave.trip(0).calibration)
        }

        function test_getCalibrationsForRowReturnsSelectionWhenRowIsInIt() {
            selectRows([1, 3, 4])

            const calibrations = calibrationsForRow(3)
            compare(calibrations.length, 3,
                    "row 3 is in a multi-selection ⇒ whole selection")
            verify(calibrations.indexOf(cave.trip(1).calibration) >= 0)
            verify(calibrations.indexOf(cave.trip(3).calibration) >= 0)
            verify(calibrations.indexOf(cave.trip(4).calibration) >= 0)
        }

        // Single-row selection counts as "no multi-select" — even when the
        // queried row matches the only selected row, scope stays single.
        function test_getCalibrationsForRowFallsBackToSelfWhenSelectionIsSingleton() {
            selectRows([2])

            const calibrations = calibrationsForRow(2)
            compare(calibrations.length, 1)
            compare(calibrations[0], cave.trip(2).calibration)
        }

        // ── The tree's trip-row menu is where the verbs live now ────────────

        // The submenu moved onto the tree with the rows: a trip row's context
        // menu carries it, and it acts on the selection the row belongs to.
        function test_tripRowMenuCarriesDeclinationOverTheSelection() {
            selectRows([0, 2, 4])

            //A row that the view has yet to re-bind after the trips were
            //rebuilt still draws the trip it last held, so the row is looked
            //up until it stands for the trip this test picked.
            let row = null
            tryVerify(() => {
                          row = findChild(cavePage(), "tripDelegate2")
                          return row !== null && row.object === cave.trip(2)
                      }, 5000, "the third trip must have a row of its own")

            row.showContextMenu(0, 0)

            let menu = null
            tryVerify(() => {
                          menu = findChild(row, "surveyItemContextMenu")
                          return menu !== null && menu.visible
                      }, 5000, "the trip row must open its context menu")

            compare(menu.count, 3, "Move to…, Delete… and Declination")
            const submenu = menu.menuAt(2)
            verify(submenu !== null, "a trip row's menu carries a submenu")
            compare(submenu.objectName, "declinationSubmenu")
            compare(menu.tripCalibrations.length, 3,
                    "the menu read the selection when it opened")
            compare(submenu.tripCalibrations.length, 3,
                    "a row inside a multi-selection covers the selection")

            findChild(submenu, autoItemName).triggered()
            menu.close()

            compare(cave.trip(0).calibration.autoDeclination, true)
            compare(cave.trip(2).calibration.autoDeclination, true)
            compare(cave.trip(4).calibration.autoDeclination, true)
            compare(cave.trip(1).calibration.autoDeclination, false, "unselected row 1 untouched")
        }

        // ── End-to-end: multi-select then trigger menu on a selected row ─────

        function test_multiSelectionFlipsAllSelectedTrips() {
            selectRows([0, 2, 4])

            rootId.standaloneCalibrations = calibrationsForRow(2)
            compare(rootId.standaloneCalibrations.length, 3,
                    "right-clicking row 2 (in selection) covers the selection")

            findChild(menuId, autoItemName).triggered()

            compare(cave.trip(0).calibration.autoDeclination, true)
            compare(cave.trip(2).calibration.autoDeclination, true)
            compare(cave.trip(4).calibration.autoDeclination, true)
            compare(cave.trip(1).calibration.autoDeclination, false, "unselected row 1 untouched")
            compare(cave.trip(3).calibration.autoDeclination, false, "unselected row 3 untouched")
        }
    }
}
