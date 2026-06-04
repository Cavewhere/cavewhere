import QtQuick
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

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=DeclCave"
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
            cavePage().selection.clear()
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

        function click(row, modifiers) {
            cavePage().applySelectionClick(row, modifiers)
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
            click(2, Qt.NoModifier) // selection = [2]

            const calibrations = cavePage().getCalibrationsForRow(0)
            compare(calibrations.length, 1, "row 0 is not in selection ⇒ single-row scope")
            compare(calibrations[0], cave.trip(0).calibration)
        }

        function test_getCalibrationsForRowReturnsSelectionWhenRowIsInIt() {
            click(1, Qt.NoModifier)
            click(3, Qt.ControlModifier)
            click(4, Qt.ControlModifier) // selection = [1, 3, 4]

            const calibrations = cavePage().getCalibrationsForRow(3)
            compare(calibrations.length, 3,
                    "row 3 is in a multi-selection ⇒ whole selection")
            verify(calibrations.indexOf(cave.trip(1).calibration) >= 0)
            verify(calibrations.indexOf(cave.trip(3).calibration) >= 0)
            verify(calibrations.indexOf(cave.trip(4).calibration) >= 0)
        }

        // Single-row selection counts as "no multi-select" — even when the
        // queried row matches the only selected row, scope stays single.
        function test_getCalibrationsForRowFallsBackToSelfWhenSelectionIsSingleton() {
            click(2, Qt.NoModifier) // selection = [2]

            const calibrations = cavePage().getCalibrationsForRow(2)
            compare(calibrations.length, 1)
            compare(calibrations[0], cave.trip(2).calibration)
        }

        // ── End-to-end: multi-select then trigger menu on a selected row ─────

        function test_multiSelectionFlipsAllSelectedTrips() {
            click(0, Qt.NoModifier)
            click(2, Qt.ControlModifier)
            click(4, Qt.ControlModifier) // selection = [0, 2, 4]

            rootId.standaloneCalibrations = cavePage().getCalibrationsForRow(2)
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
