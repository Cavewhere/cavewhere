import QtQuick
import QtTest
import cavewherelib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    PDFSettingsItem {
        objectName: "pdfSettings"
        width: 600
        height: 400
    }

    TestCase {
        name: "PDFSettingsItem"
        when: windowShown

        readonly property var settings: RootData.settings.pdfSettings

        function init() {
            settings.resetToDefaults()
            waitForRendering(rootId)
        }

        function cleanup() {
            settings.resetToDefaults()
        }

        function findResetButton() {
            return ObjectFinder.findObjectByChain(rootId, "rootId->pdfSettings->restoreDefaultsButton")
        }

        function test_restoreDefaultsButtonDisabledAtDefault() {
            let btn = findResetButton()
            verify(btn !== null, "restoreDefaultsButton not found")
            verify(!btn.enabled)
        }

        function test_restoreDefaultsButtonEnabledWhenResolutionChanged() {
            settings.resolutionImport = settings.resolutionImport + 50
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
        }

        function test_clickingRestoreDefaultsResetsValues() {
            let defaultResolution = settings.resolutionImport
            settings.resolutionImport = defaultResolution + 50
            waitForRendering(rootId)

            let btn = findResetButton()
            verify(btn !== null, "restoreDefaultsButton not found")
            verify(btn.enabled)
            mouseClick(btn)

            tryCompare(settings, "resolutionImport", defaultResolution)
        }

        function test_clickingRestoreDefaultsDisablesButton() {
            settings.resolutionImport = settings.resolutionImport + 50
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
            mouseClick(btn)
            tryVerify(function() { return !btn.enabled })
        }
    }
}
