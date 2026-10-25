import QtQuick
import QtTest
import cavewherelib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    JobSettingsItem {
        objectName: "jobSettings"
        width: 600
        height: 400
    }

    TestCase {
        name: "JobSettingsItem"
        when: windowShown

        readonly property var settings: RootData.settings.jobSettings

        function init() {
            settings.resetToDefaults()
            waitForRendering(rootId)
        }

        function cleanup() {
            settings.resetToDefaults()
        }

        function findResetButton() {
            return ObjectFinder.findObjectByChain(rootId, "rootId->jobSettings->restoreDefaultsButton")
        }

        function test_restoreDefaultsButtonDisabledAtDefault() {
            let btn = findResetButton()
            verify(btn !== null, "restoreDefaultsButton not found")
            verify(!btn.enabled)
        }

        function test_restoreDefaultsButtonEnabledWhenThreadCountChanged() {
            if (settings.idleThreadCount < 2) {
                skip("Need at least 2 usable threads to drift from default")
            }
            settings.threadCount = settings.idleThreadCount - 1
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
        }

        function test_restoreDefaultsButtonEnabledWhenAutomaticUpdateChanged() {
            settings.automaticUpdate = false
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
        }

        function test_clickingRestoreDefaultsResetsValues() {
            if (settings.idleThreadCount < 2) {
                skip("Need at least 2 usable threads to drift from default")
            }
            settings.threadCount = settings.idleThreadCount - 1
            settings.automaticUpdate = false
            waitForRendering(rootId)

            let btn = findResetButton()
            verify(btn !== null, "restoreDefaultsButton not found")
            verify(btn.enabled)
            mouseClick(btn)

            tryCompare(settings, "threadCount", settings.idleThreadCount)
            tryCompare(settings, "automaticUpdate", true)
        }

        function findMemoryBudgetSpinBox() {
            return findChild(rootId, "memoryBudgetSpinBox")
        }

        function test_memoryBudgetShowsAutomaticAtDefault() {
            let spinBox = findMemoryBudgetSpinBox()
            verify(spinBox !== null, "memoryBudgetSpinBox not found")
            compare(spinBox.value, 0)
            compare(spinBox.displayText, "Automatic")
        }

        function test_memoryBudgetIncreaseSetsTheSetting() {
            let spinBox = findMemoryBudgetSpinBox()
            verify(spinBox !== null, "memoryBudgetSpinBox not found")
            spinBox.increase()
            spinBox.valueModified()
            tryCompare(settings, "memoryBudgetMb", spinBox.stepSize)
            verify(!spinBox.displayText.startsWith("Automatic"), spinBox.displayText)
            tryVerify(function() { return findResetButton().enabled })
        }

        function test_memoryBudgetReturningToAutomaticShowsResolvedCap() {
            let item = findChild(rootId, "jobSettings")
            let spinBox = findMemoryBudgetSpinBox()
            let label = findChild(rootId, "memoryBudgetAutomaticLabel")
            verify(spinBox !== null, "memoryBudgetSpinBox not found")
            verify(label !== null, "memoryBudgetAutomaticLabel not found")
            compare(spinBox.value, 0)

            let automaticText = "(" + item.gigabytesText(settings.resolvedMemoryBudgetMb) + ")"
            tryCompare(label, "text", automaticText)

            spinBox.increase()
            spinBox.valueModified()
            tryCompare(settings, "memoryBudgetMb", spinBox.stepSize)
            verify("(" + item.gigabytesText(spinBox.stepSize) + ")" !== automaticText,
                   "manual and automatic budgets read the same")
            tryCompare(label, "text", "")

            spinBox.decrease()
            spinBox.valueModified()
            tryCompare(settings, "memoryBudgetMb", 0)
            compare(spinBox.displayText, "Automatic")
            tryCompare(label, "text", automaticText)
        }

        function test_clickingRestoreDefaultsDisablesButton() {
            settings.automaticUpdate = false
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
            mouseClick(btn)
            tryVerify(function() { return !btn.enabled })
        }
    }
}
