import QtQuick
import QtTest
import cavewherelib

// The sketch scale input's paper/cave unit fields default to the project unit
// system's natural pairing (Metric → cm / m, Imperial → in / ft) and follow the
// project live. A bound sketch scale shows its own stored units; these defaults
// only surface for an unbound input. The same pairing is seeded into new sketches
// by cwSurveyNoteSketchModel (see the [SketchDefaultScale] C++ tests).
MainWindowTest {
    id: rootId

    ScaleInput {
        id: scaleInput
        objectName: "scaleInput"
    }

    TestCase {
        name: "ScaleInput"
        when: windowShown

        function init() {
            RootData.region.unitSystem = Units.Metric
        }

        function cleanup() {
            RootData.region.unitSystem = Units.Metric
        }

        // Assert the child UnitValueInput.defaultUnit, not just the derived
        // property — that binding is the line the fallback actually flows
        // through, so reverting it must fail a test.
        function test_metricDefaultsToCentimetersAndMeters() {
            let onPaper = findChild(scaleInput, "onPaperLengthInput")
            let inCave = findChild(scaleInput, "inCaveLengthInput")
            verify(onPaper !== null)
            verify(inCave !== null)
            compare(onPaper.defaultUnit, Units.Centimeters)
            compare(inCave.defaultUnit, Units.Meters)
        }

        function test_imperialDefaultsToInchesAndFeet() {
            let onPaper = findChild(scaleInput, "onPaperLengthInput")
            let inCave = findChild(scaleInput, "inCaveLengthInput")

            RootData.region.unitSystem = Units.Imperial
            tryCompare(onPaper, "defaultUnit", Units.Inches)
            tryCompare(inCave, "defaultUnit", Units.Feet)
        }

        function test_defaultsFollowProjectLive() {
            compare(scaleInput.onPaperDefaultUnit, Units.Centimeters)

            RootData.region.unitSystem = Units.Imperial
            tryCompare(scaleInput, "onPaperDefaultUnit", Units.Inches)

            RootData.region.unitSystem = Units.Metric
            tryCompare(scaleInput, "onPaperDefaultUnit", Units.Centimeters)
        }
    }
}
