import QtQuick as QQ
import QtTest
import QtQuick.Controls as QC
import QtQuick.Controls.Fusion as FusionStyle
import cavewherelib

// SplitButton behavior: the primary button always fires clicked()
// without ever opening the menu, the chevron only exists when a menu
// is set, and clicking the chevron pops the menu. Seam styling is
// visual-only and not asserted here.
QQ.Item {
    id: rootId

    width: 400
    height: 300

    QC.Menu {
        id: menuId

        QC.MenuItem {
            objectName: "altActionItem"
            text: "Alternate action"
        }
    }

    SplitButton {
        id: splitButtonId

        buttonObjectName: "mainButton"
        text: "Add Thing"
    }

    SignalSpy {
        id: clickedSpyId
        target: splitButtonId
        signalName: "clicked"
    }

    TestCase {
        name: "SplitButton"
        when: windowShown

        function init() {
            menuId.close()
            splitButtonId.menu = null
            clickedSpyId.clear()
            tryVerify(() => !menuId.visible)
        }

        function mainButton() {
            const button = findChild(splitButtonId, "mainButton")
            verify(button !== null, "main button must exist")
            return button
        }

        function menuButton() {
            const button = findChild(splitButtonId, "menuButton")
            verify(button !== null, "chevron button must exist")
            return button
        }

        // The Row repositions the chevron a polish after it becomes
        // visible; clicking before that lands on the main button.
        function waitForChevronPlacement() {
            tryVerify(() => menuButton().visible && menuButton().x > 0, 5000,
                      "chevron is visible and positioned after the main button")
        }

        function test_primaryClickFiresWithoutOpeningMenu() {
            splitButtonId.menu = menuId

            mouseClick(mainButton())

            compare(clickedSpyId.count, 1, "primary click fires clicked() once")
            verify(!menuId.visible, "primary click never opens the menu")
        }

        function test_chevronOnlyVisibleWithMenu() {
            verify(!menuButton().visible, "no menu, no chevron")

            splitButtonId.menu = menuId
            tryVerify(() => menuButton().visible, 5000,
                      "setting a menu shows the chevron")

            splitButtonId.menu = null
            tryVerify(() => !menuButton().visible, 5000,
                      "clearing the menu hides the chevron")
        }

        function test_chevronOpensMenuWithoutFiringClicked() {
            splitButtonId.menu = menuId
            waitForChevronPlacement()

            mouseClick(menuButton())

            tryVerify(() => menuId.visible, 5000, "chevron pops the menu")
            compare(clickedSpyId.count, 0, "the chevron never fires clicked()")
        }

        function test_chevronMatchesMainButtonHeight() {
            splitButtonId.menu = menuId
            waitForChevronPlacement()
            compare(menuButton().height, mainButton().height,
                    "segments share one height")
        }

        // The seam column between the segments must read like the
        // button's visible outline, which differs by palette: on a
        // dark face it's the inner contrast highlight over the face,
        // on a light face it's the plain Fusion outline.
        function seamPixel() {
            waitForRendering(splitButtonId)
            const img = grabImage(splitButtonId)
            // grabImage returns device pixels.
            const scale = img.width / splitButtonId.width
            const x = Math.round(menuButton().x * scale)
            const y = Math.round((menuButton().height / 2) * scale)
            return Qt.rgba(img.red(x, y) / 255, img.green(x, y) / 255,
                           img.blue(x, y) / 255, 1)
        }

        function compareColor(actual, expected, message) {
            const detail = message + " (actual " + actual + " expected "
                         + expected + ")"
            fuzzyCompare(actual.r, expected.r, 1 / 255, detail)
            fuzzyCompare(actual.g, expected.g, 1 / 255, detail)
            fuzzyCompare(actual.b, expected.b, 1 / 255, detail)
        }

        function test_darkPaletteSeamMatchesPerceivedOutline() {
            // The macOS dark palette's roles, so the test exercises
            // exactly what the app renders in dark mode.
            splitButtonId.palette.window = "#1e1e1e"
            splitButtonId.palette.button = "#1f1f1f"
            splitButtonId.menu = menuId
            waitForChevronPlacement()

            const palette = menuButton().palette
            const face = FusionStyle.Fusion.buttonColor(palette, false, false, false)
            const expected = Qt.tint(face, FusionStyle.Fusion.innerContrastLine)
            const outline = FusionStyle.Fusion.buttonOutline(palette, false, true)

            const actual = seamPixel()
            compareColor(actual, expected,
                         "dark seam is the highlight-over-face tone")
            verify(Math.abs(actual.r - outline.r) > 4 / 255,
                   "dark seam is not the near-black raw outline")
        }

        function test_lightPaletteSeamMatchesOutline() {
            splitButtonId.palette.window = "#ececec"
            splitButtonId.palette.button = "#ececec"
            splitButtonId.menu = menuId
            waitForChevronPlacement()

            const expected = FusionStyle.Fusion.buttonOutline(
                menuButton().palette, false, true)
            compareColor(seamPixel(), expected,
                         "light seam is the plain Fusion outline")
        }
    }
}
