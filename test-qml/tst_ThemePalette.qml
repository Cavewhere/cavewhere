import QtQuick
import QtQuick.Controls as QC
import QtTest
import cavewherelib

MainWindowTest {
    id: rootId

    // The application's window type. Its palette comes from the style's
    // ApplicationWindow, the same way CavewhereMainWindow gets it.
    Component {
        id: applicationWindowComponent
        QC.ApplicationWindow {
            property alias field: fieldId
            property alias disabledField: disabledFieldId
            property alias menu: menuId
            property alias menuItem: menuItemId
            property alias popup: popupId
            property alias popupField: popupFieldId

            width: 320
            height: 240
            visible: true

            QC.TextField { id: fieldId }
            QC.TextField { id: disabledFieldId; enabled: false }
            QC.Menu {
                id: menuId
                QC.MenuItem { id: menuItemId; text: "Copy" }
            }
            QC.Popup {
                id: popupId
                QC.TextField { id: popupFieldId }
            }
        }
    }

    TestCase {
        name: "ThemePalette"
        when: windowShown

        function createObject(qml) {
            return createTemporaryQmlObject("import QtQuick.Controls as QC; " + qml, rootId)
        }

        function createWindow() {
            const window = createTemporaryObject(applicationWindowComponent, rootId)
            tryVerify(() => window.visible)
            return window
        }

        function flipScheme() {
            const startedDark = Theme.dark
            RootData.settings.appearanceSettings.colorScheme =
                    startedDark ? AppearanceSettings.Light : AppearanceSettings.Dark
            tryVerify(() => Theme.dark !== startedDark)
        }

        function verifyFollowsTheme(palette, what) {
            tryVerify(() => Qt.colorEqual(palette.highlight, Theme.highlight), 5000, what + " highlight")
            tryVerify(() => Qt.colorEqual(palette.accent, Theme.accent), 5000, what + " accent")
            tryVerify(() => Qt.colorEqual(palette.base, Theme.fieldSurface), 5000, what + " base")
            tryVerify(() => Qt.colorEqual(palette.text, Theme.text), 5000, what + " text")
            tryVerify(() => Qt.colorEqual(palette.windowText, Theme.text), 5000, what + " windowText")
        }

        function verifyWindowFollowsTheme(window, when) {
            verifyFollowsTheme(window.field.palette, "a text field " + when)
            verifyFollowsTheme(window.menu.palette, "a menu " + when)
            verifyFollowsTheme(window.popup.palette, "a popup " + when)

            window.menu.open()
            tryVerify(() => window.menu.opened)
            verifyFollowsTheme(window.menuItem.palette, "a menu item " + when)
            window.menu.close()

            window.popup.open()
            tryVerify(() => window.popup.opened)
            verifyFollowsTheme(window.popupField.palette, "a text field in a popup " + when)
            window.popup.close()
        }

        function cleanup() {
            RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.System
        }

        // A control starts from the fallback style's own palette; the Theme
        // colors have to reach it with no scheme change to nudge them.
        function test_applicationWindowFollowsThemeAtStartup() {
            verifyWindowFollowsTheme(createWindow(), "at startup")
        }

        function test_applicationWindowFollowsSchemeChange() {
            const window = createWindow()
            verifyFollowsTheme(window.menu.palette, "a menu before the scheme change")
            flipScheme()
            verifyWindowFollowsTheme(window, "after the scheme change")
        }

        function test_disabledTextIsDimmed() {
            const window = createWindow()
            tryVerify(() => Qt.colorEqual(window.disabledField.palette.text, Theme.textDisabled),
                      5000, "disabled text")
            verify(!Qt.colorEqual(Theme.textDisabled, Theme.text))
        }

        // The test window is a plain view that MainWindowTest hands the palette.
        function test_harnessControlFollowsTheme() {
            verifyFollowsTheme(createObject("QC.TextField {}").palette, "a new text field")
            flipScheme()
            verifyFollowsTheme(createObject("QC.TextField {}").palette, "a text field created after the scheme change")
        }

        function test_harnessPopupFollowsTheme() {
            const popup = createObject("QC.Popup { QC.TextField {} }")
            popup.open()
            tryVerify(() => popup.opened)
            verifyFollowsTheme(popup.palette, "a new popup")
            verifyFollowsTheme(popup.contentItem.children[0].palette, "a text field in a new popup")
            popup.close()
        }
    }
}
