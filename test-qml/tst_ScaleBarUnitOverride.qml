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
            scaleBar.unitMode = Units.FollowProject
            waitForRendering(rootId)
        }

        function cleanup() {
            RootData.region.unitSystem = Units.Metric
            scaleBar.unitMode = Units.FollowProject
        }

        function test_followsProjectByDefault() {
            compare(scaleBar.unitMode, Units.FollowProject)
            compare(scaleBar.unitSystem, Units.Metric)

            RootData.region.unitSystem = Units.Imperial
            tryCompare(scaleBar, "unitSystem", Units.Imperial)
        }

        function test_sessionOverrideWinsOverProject() {
            RootData.region.unitSystem = Units.Imperial
            tryCompare(scaleBar, "unitSystem", Units.Imperial)

            scaleBar.unitMode = Units.ForceMetric
            compare(scaleBar.unitSystem, Units.Metric)

            // The project is untouched by the session override.
            compare(RootData.region.unitSystem, Units.Imperial)
        }

        function test_clearingOverrideFollowsProjectAgain() {
            RootData.region.unitSystem = Units.Imperial
            scaleBar.unitMode = Units.ForceMetric
            compare(scaleBar.unitSystem, Units.Metric)

            scaleBar.unitMode = Units.FollowProject
            tryCompare(scaleBar, "unitSystem", Units.Imperial)
        }

        // The menu offers only the unit the bar is NOT already showing: the
        // item for the current system is disabled (pinning it is a no-op).
        function test_currentSystemMenuItemDisabled() {
            let metricItem = findChild(scaleBar, "scaleBarMetricMenuItem")
            let imperialItem = findChild(scaleBar, "scaleBarImperialMenuItem")
            verify(metricItem !== null)
            verify(imperialItem !== null)

            // FollowProject + metric project -> showing metric.
            compare(scaleBar.unitSystem, Units.Metric)
            verify(!metricItem.enabled)
            verify(imperialItem.enabled)

            // Following an imperial project flips which item is offered.
            RootData.region.unitSystem = Units.Imperial
            tryCompare(scaleBar, "unitSystem", Units.Imperial)
            verify(metricItem.enabled)
            verify(!imperialItem.enabled)

            // A force pin disables its own item regardless of the project.
            scaleBar.unitMode = Units.ForceMetric
            compare(scaleBar.unitSystem, Units.Metric)
            verify(!metricItem.enabled)
            verify(imperialItem.enabled)
        }

        // The project-default unit's item is annotated, and choosing it returns
        // the bar to following the project rather than pinning that unit.
        function test_projectDefaultItemFollowsProject() {
            let metricItem = findChild(scaleBar, "scaleBarMetricMenuItem")
            let imperialItem = findChild(scaleBar, "scaleBarImperialMenuItem")

            RootData.region.unitSystem = Units.Metric
            tryCompare(scaleBar, "unitSystem", Units.Metric)
            verify(metricItem.text.indexOf("Project Default") !== -1)
            verify(imperialItem.text.indexOf("Project Default") === -1)

            // Pin imperial (project stays metric), then the annotated Metric item
            // returns to following — mode FollowProject, not ForceMetric.
            scaleBar.unitMode = Units.ForceImperial
            compare(scaleBar.unitSystem, Units.Imperial)
            metricItem.triggered()
            compare(scaleBar.unitMode, Units.FollowProject)
            compare(scaleBar.unitSystem, Units.Metric)

            // The non-default unit's item pins (ForceImperial), not follow.
            imperialItem.triggered()
            compare(scaleBar.unitMode, Units.ForceImperial)
        }
    }
}
