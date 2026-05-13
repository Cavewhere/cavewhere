import QtQuick
import QtTest
import cavewherelib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    WarpingSettingsItem {
        objectName: "warpingSettings"
        width: 600
        height: 700
        warpingSettings: RootData.scrapManager.warpingSettings
    }

    TestCase {
        name: "WarpingSettingsItem"
        when: windowShown

        readonly property var settings: RootData.scrapManager.warpingSettings

        function init() {
            settings.resetToDefaults()
            waitForRendering(rootId)
        }

        function cleanup() {
            settings.resetToDefaults()
        }

        function findResetButton() {
            return ObjectFinder.findObjectByChain(rootId, "rootId->warpingSettings->GroupBox->restoreDefaultsButton")
        }

        function test_restoreDefaultsButtonDisabledAtDefault() {
            let btn = findResetButton()
            verify(btn !== null, "restoreDefaultsButton not found")
            verify(!btn.enabled)
        }

        function test_restoreDefaultsButtonEnabledWhenGridResolutionChanged() {
            settings.gridResolutionMeters = settings.gridResolutionMeters + 1.0
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
        }

        function test_restoreDefaultsButtonEnabledWhenBooleanToggled() {
            settings.useSmoothingRadius = !settings.useSmoothingRadius
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
        }

        function test_clickingRestoreDefaultsResetsAllValues() {
            let defaultGrid = settings.gridResolutionMeters
            let defaultMaxStations = settings.maxClosestStations
            let defaultSmoothing = settings.smoothingRadiusMeters
            let defaultUseSmoothing = settings.useSmoothingRadius

            settings.gridResolutionMeters = defaultGrid + 1.0
            settings.maxClosestStations = defaultMaxStations + 5
            settings.smoothingRadiusMeters = defaultSmoothing + 1.0
            settings.useSmoothingRadius = !defaultUseSmoothing
            waitForRendering(rootId)

            let btn = findResetButton()
            verify(btn !== null, "restoreDefaultsButton not found")
            verify(btn.enabled)
            mouseClick(btn)

            tryCompare(settings, "gridResolutionMeters", defaultGrid)
            tryCompare(settings, "maxClosestStations", defaultMaxStations)
            tryCompare(settings, "smoothingRadiusMeters", defaultSmoothing)
            tryCompare(settings, "useSmoothingRadius", defaultUseSmoothing)
        }

        function test_clickingRestoreDefaultsDisablesButton() {
            settings.useSmoothingRadius = !settings.useSmoothingRadius
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
            mouseClick(btn)
            tryVerify(function() { return !btn.enabled })
        }
    }
}
