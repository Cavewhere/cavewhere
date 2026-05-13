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

        function test_clickingRestoreDefaultsDisablesButton() {
            settings.automaticUpdate = false
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
            mouseClick(btn)
            tryVerify(function() { return !btn.enabled })
        }
    }
}
