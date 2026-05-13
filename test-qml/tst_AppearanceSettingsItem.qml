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

        readonly property string defaultFontFamily: RootData.settings.fontSettings.fontEntries[0].family

        function resetToDefaults() {
            RootData.settings.fontSettings.fontFamily  = defaultFontFamily
            RootData.settings.fontSettings.fontBaseSize = RootData.settings.fontSettings.defaultFontBaseSize
        }

        function init() {
            resetToDefaults()
            waitForRendering(rootId)
        }

        function cleanup() {
            resetToDefaults()
        }

        // ── Font family ──────────────────────────────────────────────────────

        function test_cavewhereRadioButtonCheckedByDefault() {
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->CaveWhereRadioButton")
            verify(btn !== null, "CaveWhereRadioButton not found")
            verify(btn.checked)
        }

        function test_selectFiraSansFont() {
            let btn = ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->GroupBox->FiraSansRadioButton")
            verify(btn !== null, "FiraSansRadioButton not found")
            mouseClick(btn)
            compare(RootData.settings.fontSettings.fontFamily, "Fira Sans")
            compare(RootData.settings.fontSettings.defaultFontBaseSize, 14)
            compare(RootData.settings.fontSettings.fontBaseSize, 14)
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

        // ── Restore Defaults button ───────────────────────────────────────────

        function findResetButton() {
            return ObjectFinder.findObjectByChain(rootId, "rootId->appearanceSettings->restoreDefaultsButton")
        }

        function test_restoreDefaultsButtonDisabledAtDefault() {
            let btn = findResetButton()
            verify(btn !== null, "restoreDefaultsButton not found")
            verify(!btn.enabled)
        }

        function test_restoreDefaultsButtonEnabledWhenSizeChanged() {
            RootData.settings.fontSettings.fontBaseSize = RootData.settings.fontSettings.defaultFontBaseSize + 2
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
        }

        function test_restoreDefaultsButtonEnabledWhenFamilyChanged() {
            let entries = RootData.settings.fontSettings.fontEntries
            if (entries.length < 2) {
                skip("Need at least 2 font entries to switch family")
            }
            RootData.settings.fontSettings.fontFamily = entries[1].family
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
        }

        function test_clickingRestoreDefaultsResetsValues() {
            let entries = RootData.settings.fontSettings.fontEntries
            let defaultFamily = entries[0].family
            let defaultSize = entries[0].defaultSize

            if (entries.length >= 2) {
                RootData.settings.fontSettings.fontFamily = entries[1].family
            }
            RootData.settings.fontSettings.fontBaseSize = RootData.settings.fontSettings.fontBaseSize + 2
            waitForRendering(rootId)

            let btn = findResetButton()
            verify(btn !== null, "restoreDefaultsButton not found")
            verify(btn.enabled)
            mouseClick(btn)

            tryCompare(RootData.settings.fontSettings, "fontFamily", defaultFamily)
            tryCompare(RootData.settings.fontSettings, "fontBaseSize", defaultSize)
        }

        function test_clickingRestoreDefaultsDisablesButton() {
            RootData.settings.fontSettings.fontBaseSize = RootData.settings.fontSettings.defaultFontBaseSize + 2
            let btn = findResetButton()
            tryVerify(function() { return btn.enabled })
            mouseClick(btn)
            tryVerify(function() { return !btn.enabled })
        }
    }
}
