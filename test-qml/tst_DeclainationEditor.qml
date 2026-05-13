import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    property var calibration: null

    DeclainationEditor {
        id: editorId
        objectName: "declinationEditor"
        width: 400
        calibration: rootId.calibration
    }

    TestCase {
        name: "DeclainationEditor"
        when: windowShown

        function setupTrip(withFixStation, withDate) {
            RootData.newProject()

            RootData.region.addCave()
            const cave = RootData.region.cave(0)
            cave.name = "BoulderCave"

            if (withFixStation) {
                cave.fixStations.addFixStation()
                const fixModel = cave.fixStations
                const idx = fixModel.index(0)
                fixModel.setData(idx, "a1", FixStationModel.StationNameRole)
                fixModel.setData(idx, "EPSG:32613", FixStationModel.InputCSRole)
                fixModel.setData(idx, 478000.0, FixStationModel.EastingRole)
                fixModel.setData(idx, 4430000.0, FixStationModel.NorthingRole)
                fixModel.setData(idx, 1655.0, FixStationModel.ElevationRole)
            }

            cave.addTrip()
            const trip = cave.trip(0)
            trip.name = "BoulderTrip"
            if (withDate) {
                trip.date = new Date(2024, 5, 1) // June 1 2024
            } else {
                trip.date = new Date(NaN) // invalid date
            }

            rootId.calibration = trip.calibration
            return trip
        }

        function cleanup() {
            rootId.calibration = null
            RootData.newProject()
        }

        // ── Seams ────────────────────────────────────────────────────────────

        function test_objectNameSeamsExist() {
            setupTrip(true, true)

            verify(findChild(editorId, "declinationEdit") !== null,
                   "declinationEdit must exist")
            verify(findChild(editorId, "declinationModeCombo") !== null,
                   "declinationModeCombo must exist")
            verify(findChild(editorId, "declinationWarningIcon") !== null,
                   "declinationWarningIcon must exist")
        }

        // ── Mode combo: Auto → Manual ────────────────────────────────────────

        function test_modeComboSwitchesToManual() {
            const trip = setupTrip(true, true)
            const calibration = trip.calibration

            verify(calibration.autoDeclination)
            verify(calibration.autoDeclinationAvailable)

            const combo = findChild(editorId, "declinationModeCombo")
            const valueInput = findChild(editorId, "declinationEdit")

            tryVerify(() => combo.visible, 500, "mode combo should be visible")
            compare(combo.currentIndex, 0, "should start in Auto")
            tryVerify(() => valueInput.readOnly, 500,
                      "value input should be read-only in Auto mode")

            combo.activated(1)

            tryVerify(() => !calibration.autoDeclination, 500,
                      "selecting Manual should clear autoDeclination")
            tryVerify(() => !valueInput.readOnly, 500,
                      "value input should be editable after switch")
        }

        // ── Mode combo: Manual → Auto ────────────────────────────────────────

        function test_modeComboSwitchesToAuto() {
            const trip = setupTrip(true, true)
            const calibration = trip.calibration
            calibration.autoDeclination = false

            const combo = findChild(editorId, "declinationModeCombo")
            const valueInput = findChild(editorId, "declinationEdit")

            tryVerify(() => combo.currentIndex === 1, 500, "should reflect Manual")
            tryVerify(() => !valueInput.readOnly, 500,
                      "value input should be editable in Manual mode")

            combo.activated(0)

            tryVerify(() => calibration.autoDeclination, 500,
                      "selecting Auto should set autoDeclination=true")
            tryVerify(() => valueInput.readOnly, 500,
                      "value input should become read-only after switch")
        }

        // ── Combo hidden when auto unavailable ───────────────────────────────

        function test_modeComboHiddenWithoutFixStation() {
            const trip = setupTrip(false, true) // no fix station
            const calibration = trip.calibration

            tryVerify(() => !calibration.autoDeclinationAvailable, 500,
                      "without a fix station auto must be unavailable")

            const combo = findChild(editorId, "declinationModeCombo")
            const valueInput = findChild(editorId, "declinationEdit")
            // Nothing to choose between — combo collapses and the value input
            // stays editable.
            tryVerify(() => !combo.visible, 500, "mode combo hidden when auto unavailable")
            tryVerify(() => !valueInput.readOnly, 500,
                      "value input editable when auto unavailable")
        }

        // ── Value tracks trip date in Auto mode ──────────────────────────────

        function test_valueUpdatesWhenTripDateChanges() {
            const trip = setupTrip(true, true)
            const calibration = trip.calibration

            verify(calibration.autoDeclinationAvailable)

            const valueInput = findChild(editorId, "declinationEdit")
            tryVerify(() => valueInput.readOnly, 500)

            const initialText = valueInput.text
            const initialValue = calibration.declination

            // IGRF drift is ~0.1°/yr at this latitude, so a 44-year jump must
            // move the resolved value (and the displayed text) measurably.
            trip.date = new Date(1980, 0, 1)
            tryVerify(() => Math.abs(calibration.declination - initialValue) > 1.0,
                      500, "declination must drift after 44-year date change")
            tryVerify(() => valueInput.text !== initialText, 500,
                      "displayed value should update with the date change")
        }

        // ── Manual edit writes to declinationManual ──────────────────────────

        function test_manualEditWritesToDeclinationManual() {
            const trip = setupTrip(true, true)
            const calibration = trip.calibration
            calibration.autoDeclination = false
            calibration.declinationManual = 5.0

            const valueInput = findChild(editorId, "declinationEdit")
            tryVerify(() => !valueInput.readOnly, 500)

            valueInput.finishedEditting("9.50")
            tryVerify(() => Math.abs(calibration.declinationManual - 9.5) < 1e-6, 500,
                      "manual edit must update declinationManual, not declination")
        }

        // ── Warning icon hidden by default ───────────────────────────────────

        function test_warningIconHiddenWithoutWarning() {
            setupTrip(true, true)

            const warningIcon = findChild(editorId, "declinationWarningIcon")
            verify(warningIcon !== null)
            verify(!warningIcon.visible,
                   "warning icon must stay hidden until phase 6 wires up warnings")
        }
    }
}
