import QtQuick
import QtTest
import cavewherelib
import QmlTestRecorder

// The on-screen view scale bar (#133) is no longer locked to metres: it follows
// the project unit system, and a right-click session override can pin this one
// bar to Metric/Imperial without touching the project. This exercises that unit
// resolution directly (the property state the right-click menu drives).
MainWindowTest {
    id: rootId

    Camera { id: testCamera }

    Item {
        width: 400
        height: 200

        ScaleBar {
            id: scaleBar
            objectName: "scaleBar"
            camera: testCamera
        }
    }

    TestCase {
        name: "ScaleBarUnitOverride"
        when: windowShown

        function init() {
            RootData.region.unitSystem = Units.Metric
            scaleBar.hasSessionOverride = false
            waitForRendering(rootId)
        }

        function cleanup() {
            RootData.region.unitSystem = Units.Metric
            scaleBar.hasSessionOverride = false
        }

        function test_followsProjectByDefault() {
            verify(!scaleBar.hasSessionOverride)
            compare(scaleBar.unitSystem, Units.Metric)

            RootData.region.unitSystem = Units.Imperial
            tryCompare(scaleBar, "unitSystem", Units.Imperial)
        }

        function test_sessionOverrideWinsOverProject() {
            RootData.region.unitSystem = Units.Imperial
            tryCompare(scaleBar, "unitSystem", Units.Imperial)

            scaleBar.sessionUnitSystem = Units.Metric
            scaleBar.hasSessionOverride = true
            compare(scaleBar.unitSystem, Units.Metric)

            // The project is untouched by the session override.
            compare(RootData.region.unitSystem, Units.Imperial)
        }

        function test_clearingOverrideFollowsProjectAgain() {
            RootData.region.unitSystem = Units.Imperial
            scaleBar.sessionUnitSystem = Units.Metric
            scaleBar.hasSessionOverride = true
            compare(scaleBar.unitSystem, Units.Metric)

            scaleBar.hasSessionOverride = false
            tryCompare(scaleBar, "unitSystem", Units.Imperial)
        }
    }
}
