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
        readonly property PassageTubeSettings passageTubeSettings: RootData.settings.passageTubeSettings

        // The screen-space error lives in a QC.SpinBox scaled by this much,
        // since a SpinBox counts in integers.
        readonly property int screenSpaceErrorScale: 10
        readonly property int sampleSpacingScale: 10

        function init() {
            settings.resetToDefaults()
            passageTubeSettings.resetToDefaults()
            waitForRendering(rootId)
        }

        function cleanup() {
            settings.resetToDefaults()
            passageTubeSettings.resetToDefaults()
        }

        function findComboBox() {
            return ObjectFinder.findObjectByChain(rootId, "rootId->renderingSettings->GroupBox->msaaSampleCountComboBox")
        }

        function findSpinBox(name) {
            let found = ObjectFinder.findObjectByChain(
                rootId, "rootId->renderingSettings->GroupBox->" + name)
            verify(found !== null, name + " not found")
            return found
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

        function test_budgetSpinBoxesShowTheCurrentSettings() {
            compare(findSpinBox("gpuMemoryBudgetSpinBox").value, settings.gpuMemoryBudgetMb)
            compare(findSpinBox("cpuCacheBudgetSpinBox").value, settings.cpuCacheBudgetMb)
            compare(findSpinBox("uploadBudgetSpinBox").value, settings.uploadBudgetMbPerFrame)
            compare(findSpinBox("screenSpaceErrorSpinBox").value,
                    Math.round(settings.screenSpaceErrorPx * screenSpaceErrorScale))
        }

        function test_budgetSpinBoxesTrackExternalChanges() {
            let cpu = findSpinBox("cpuCacheBudgetSpinBox")
            let upload = findSpinBox("uploadBudgetSpinBox")
            let error = findSpinBox("screenSpaceErrorSpinBox")

            settings.cpuCacheBudgetMb = 1024
            settings.uploadBudgetMbPerFrame = 16
            settings.screenSpaceErrorPx = 3.5

            tryCompare(cpu, "value", 1024)
            tryCompare(upload, "value", 16)
            tryCompare(error, "value", 35)

            settings.resetToDefaults()

            tryCompare(cpu, "value", settings.cpuCacheBudgetMb)
            tryCompare(upload, "value", settings.uploadBudgetMbPerFrame)
            tryCompare(error, "value",
                       Math.round(settings.screenSpaceErrorPx * screenSpaceErrorScale))
        }

        function test_editingTheBudgetSpinBoxesWritesTheSettings() {
            let cpu = findSpinBox("cpuCacheBudgetSpinBox")
            let upload = findSpinBox("uploadBudgetSpinBox")
            let error = findSpinBox("screenSpaceErrorSpinBox")

            //What a SpinBox does when the user spins or edits it
            cpu.value = 2048
            cpu.valueModified()
            upload.value = 32
            upload.valueModified()
            error.value = 25
            error.valueModified()

            tryCompare(settings, "cpuCacheBudgetMb", 2048)
            tryCompare(settings, "uploadBudgetMbPerFrame", 32)
            tryCompare(settings, "screenSpaceErrorPx", 2.5)
        }

        function test_passageTubeSpinBoxesTrackTheSettings() {
            let spacing = findSpinBox("sampleSpacingSpinBox")
            let resolution = findSpinBox("ringResolutionSpinBox")

            compare(spacing.value, Math.round(passageTubeSettings.sampleSpacingMeters * sampleSpacingScale))
            compare(resolution.value, passageTubeSettings.ringResolution)

            passageTubeSettings.sampleSpacingMeters = 2.5
            passageTubeSettings.ringResolution = 32
            tryCompare(spacing, "value", 25)
            tryCompare(resolution, "value", 32)

            passageTubeSettings.resetToDefaults()
            tryCompare(spacing, "value", 10)
            tryCompare(resolution, "value", 16)
        }

        function test_passageTubeRestoreDefaultsResetsTheSettings() {
            let button = ObjectFinder.findObjectByChain(
                rootId, "rootId->renderingSettings->GroupBox->passageTubeRestoreDefaults->restoreDefaultsButton")
            verify(button !== null, "passage tube restore defaults button not found")
            verify(!button.enabled, "the button is disabled at the defaults")

            passageTubeSettings.ringResolution = 32
            tryCompare(button, "enabled", true)

            mouseClick(button)
            tryCompare(passageTubeSettings, "ringResolution", 16)
            tryCompare(button, "enabled", false)
        }

        function test_editingThePassageTubeSpinBoxesWritesTheSettings() {
            let spacing = findSpinBox("sampleSpacingSpinBox")
            let resolution = findSpinBox("ringResolutionSpinBox")

            spacing.value = 35
            spacing.valueModified()
            resolution.value = 24
            resolution.valueModified()

            tryCompare(passageTubeSettings, "sampleSpacingMeters", 3.5)
            tryCompare(passageTubeSettings, "ringResolution", 24)
        }
    }
}
