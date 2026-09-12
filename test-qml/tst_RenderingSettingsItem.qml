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

        // The screen-space error lives in a QC.SpinBox scaled by this much,
        // since a SpinBox counts in integers.
        readonly property int screenSpaceErrorScale: 10

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

        // The ranges belong to the settings object, so the spin boxes can only
        // offer values the setters accept.
        function test_spinBoxRangesComeFromSettings() {
            let gpu = findSpinBox("gpuMemoryBudgetSpinBox")
            compare(gpu.from, settings.minimumGpuMemoryBudgetMb)
            compare(gpu.to, settings.maximumGpuMemoryBudgetMb)

            let cpu = findSpinBox("cpuCacheBudgetSpinBox")
            compare(cpu.from, settings.minimumCpuCacheBudgetMb)
            compare(cpu.to, settings.maximumCpuCacheBudgetMb)

            let upload = findSpinBox("uploadBudgetSpinBox")
            compare(upload.from, settings.minimumUploadBudgetMbPerFrame)
            compare(upload.to, settings.maximumUploadBudgetMbPerFrame)

            let error = findSpinBox("screenSpaceErrorSpinBox")
            compare(error.from,
                    Math.round(settings.minimumScreenSpaceErrorPx * screenSpaceErrorScale))
            compare(error.to,
                    Math.round(settings.maximumScreenSpaceErrorPx * screenSpaceErrorScale))
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
    }
}
