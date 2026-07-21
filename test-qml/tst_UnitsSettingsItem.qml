import QtQuick
import QtTest
import cavewherelib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    UnitsSettingsItem {
        objectName: "unitSettings"
        width: 600
        height: 400
    }

    TestCase {
        name: "UnitsSettingsItem"
        when: windowShown

        readonly property var settings: RootData.settings.unitSettings

        function init() {
            settings.unitSystem = Units.Metric
            waitForRendering(rootId)
        }

        function cleanup() {
            settings.unitSystem = Units.Metric
        }

        function findComboBox() {
            return ObjectFinder.findObjectByChain(rootId, "rootId->unitSettings->GroupBox->unitSystemComboBox")
        }

        function test_comboBoxShowsCurrentDefault() {
            let combo = findComboBox()
            verify(combo !== null, "unitSystemComboBox not found")
            // Metric is index 0 in the enum and the combobox model.
            compare(combo.currentIndex, Units.Metric)
        }

        function test_comboBoxTracksExternalChange() {
            let combo = findComboBox()
            verify(combo !== null, "unitSystemComboBox not found")

            settings.unitSystem = Units.Imperial
            tryCompare(combo, "currentIndex", Units.Imperial)

            settings.unitSystem = Units.Metric
            tryCompare(combo, "currentIndex", Units.Metric)
        }

        function test_selectingWritesThroughToSettings() {
            let combo = findComboBox()
            verify(combo !== null, "unitSystemComboBox not found")

            // Simulate the user choosing Imperial from the drop-down. Emitting
            // activated() with the index (rather than assigning currentIndex)
            // exercises the handler without destroying the currentIndex binding,
            // so this test is order-independent.
            combo.activated(Units.Imperial)
            compare(settings.unitSystem, Units.Imperial)
        }
    }
}
