import QtQuick
import QtTest
import cavewherelib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    RenderingSettingsItem {
        objectName: "renderingSettings"
        width: 600
        height: 400
    }

    TestCase {
        name: "RenderingSettingsItem"
        when: windowShown

        readonly property var settings: RootData.settings.renderingSettings

        function init() {
            settings.resetToDefaults()
            waitForRendering(rootId)
        }

        function cleanup() {
            settings.resetToDefaults()
        }

        function findComboBox() {
            return ObjectFinder.findObjectByChain(rootId, "rootId->renderingSettings->GroupBox->msaaSampleCountComboBox")
        }

        // The combo box must show the persisted default (4x) on first load, not
        // the model's index-0 entry. This confirms the reported failure where the
        // box does not populate the correct default.
        function test_comboBoxShowsDefaultSampleCount() {
            compare(settings.sampleCount, 4, "precondition: default sampleCount is 4")

            let combo = findComboBox()
            verify(combo !== null, "msaaSampleCountComboBox not found")

            compare(combo.currentValue, 4, "combo box should show the 4x default")
            // Default supported set is {1, 2, 4, 8}, so 4x is index 2.
            compare(combo.currentIndex, 2, "4x is index 2 in the model")
        }

        function test_comboBoxTracksExternalChange() {
            let combo = findComboBox()
            verify(combo !== null, "msaaSampleCountComboBox not found")

            settings.sampleCount = 8
            tryCompare(combo, "currentValue", 8)

            settings.resetToDefaults()
            tryCompare(combo, "currentValue", 4)
        }
    }
}
