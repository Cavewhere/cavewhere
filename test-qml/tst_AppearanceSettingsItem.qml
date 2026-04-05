import QtQuick
import QtTest
import cavewherelib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    AppearanceSettingsItem {
        objectName: "appearanceSettings"
        width: 600
        height: 500
    }

    TestCase {
        name: "AppearanceSettingsItem"
        when: windowShown

        property int savedFontBaseSize
        property string savedFontFamily

        function init() {
            savedFontBaseSize = RootData.settings.fontSettings.fontBaseSize
            savedFontFamily   = RootData.settings.fontSettings.fontFamily
            RootData.settings.fontSettings.fontBaseSize = RootData.settings.fontSettings.defaultFontBaseSize
            RootData.settings.fontSettings.fontFamily   = "Yanone Kaffeesatz"
            waitForRendering(rootId)
        }

        function cleanup() {
            RootData.settings.fontSettings.fontBaseSize = savedFontBaseSize
            RootData.settings.fontSettings.fontFamily   = savedFontFamily
        }

        // ── Font family ──────────────────────────────────────────────────────

        function test_cavewhereRadioButtonCheckedByDefault() {
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->CaveWhereRadioButton")
            verify(btn !== null, "CaveWhereRadioButton not found")
            verify(btn.checked)
        }

        function test_selectVeraSansFont() {
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->VeraSansRadioButton")
            verify(btn !== null, "VeraSansRadioButton not found")
            mouseClick(btn)
            compare(RootData.settings.fontSettings.fontFamily, "Bitstream Vera Sans")
        }

        function test_selectSystemFont() {
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->SystemRadioButton")
            verify(btn !== null, "SystemRadioButton not found")
            mouseClick(btn)
            compare(RootData.settings.fontSettings.fontFamily, "")
        }

        function test_selectCaveWhereFont() {
            RootData.settings.fontSettings.fontFamily = ""
            waitForRendering(rootId)
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->CaveWhereRadioButton")
            mouseClick(btn)
            compare(RootData.settings.fontSettings.fontFamily, "Yanone Kaffeesatz")
        }

        // ── Size buttons ─────────────────────────────────────────────────────

        function test_smallerButtonDecrementsSize() {
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->smallerButton")
            verify(btn !== null, "smallerButton not found")
            let before = RootData.settings.fontSettings.fontBaseSize
            mouseClick(btn)
            compare(RootData.settings.fontSettings.fontBaseSize, before - 2)
        }

        function test_largerButtonIncrementsSize() {
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->largerButton")
            verify(btn !== null, "largerButton not found")
            let before = RootData.settings.fontSettings.fontBaseSize
            mouseClick(btn)
            compare(RootData.settings.fontSettings.fontBaseSize, before + 2)
        }

        function test_defaultButtonResetsSize() {
            RootData.settings.fontSettings.fontBaseSize = RootData.settings.fontSettings.defaultFontBaseSize + 4
            waitForRendering(rootId)
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->defaultCard->defaultButton")
            verify(btn !== null, "defaultButton not found")
            mouseClick(btn)
            compare(RootData.settings.fontSettings.fontBaseSize, RootData.settings.fontSettings.defaultFontBaseSize)
        }

        // ── Button enabled states ─────────────────────────────────────────────

        function test_defaultButtonDisabledAtDefault() {
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->defaultCard->defaultButton")
            verify(btn !== null, "defaultButton not found")
            verify(!btn.enabled)
        }

        function test_defaultButtonEnabledWhenNotAtDefault() {
            RootData.settings.fontSettings.fontBaseSize = RootData.settings.fontSettings.defaultFontBaseSize + 2
            waitForRendering(rootId)
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->defaultCard->defaultButton")
            verify(btn.enabled)
        }

        function test_smallerButtonDisabledAtMin() {
            RootData.settings.fontSettings.fontBaseSize = 10
            waitForRendering(rootId)
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->smallerButton")
            verify(!btn.enabled)
        }

        function test_largerButtonDisabledAtMax() {
            RootData.settings.fontSettings.fontBaseSize = 28
            waitForRendering(rootId)
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->largerButton")
            verify(!btn.enabled)
        }

        // ── Default card active border ────────────────────────────────────────

        function test_defaultCardActiveAtDefault() {
            let card = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->defaultCard")
            verify(card !== null, "defaultCard not found")
            verify(card.active)
        }

        function test_defaultCardInactiveWhenNotAtDefault() {
            RootData.settings.fontSettings.fontBaseSize = RootData.settings.fontSettings.defaultFontBaseSize + 2
            waitForRendering(rootId)
            let card = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->defaultCard")
            verify(!card.active)
        }
    }
}
