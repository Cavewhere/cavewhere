import QtQuick as QQ
import QtTest
import QtQuick.Controls as QC
import cavewherelib

// SplitButton behavior: the primary button always fires clicked()
// without ever opening the menu, the chevron only exists when a menu
// is set, clicking the chevron pops the menu, and both segments draw
// the CaveWhereStyle button panel.
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

    QC.Button {
        id: referenceButtonId

        y: 100
        text: "Reference"
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
            splitButtonId.enabled = true
            mainButton().hoverEnabled = Qt.styleHints.useHoverEffects
            clickedSpyId.clear()
            mouseMove(rootId, rootId.width - 1, rootId.height - 1)
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

        function compareColor(actual, expected, message) {
            const detail = message + " (actual " + actual + " expected "
                         + expected + ")"
            fuzzyCompare(actual.r, expected.r, 1 / 255, detail)
            fuzzyCompare(actual.g, expected.g, 1 / 255, detail)
            fuzzyCompare(actual.b, expected.b, 1 / 255, detail)
        }

        // Both segments draw the CaveWhereStyle button panel: the same
        // surface, border, radius, height, and font as a plain style Button.
        function test_segmentsMatchStyleButton() {
            splitButtonId.menu = menuId
            waitForChevronPlacement()

            for (const segment of [mainButton(), menuButton()]) {
                const panel = segment.background
                compareColor(panel.color, referenceButtonId.background.color,
                             "segment surface matches a style Button")
                compareColor(panel.border.color, referenceButtonId.background.border.color,
                             "segment border matches a style Button")
                compare(panel.border.width, referenceButtonId.background.border.width,
                        "segment border width matches a style Button")
                compare(panel.radius, Theme.controlRadius, "segment uses the style's control radius")
                compare(segment.height, referenceButtonId.height,
                        "segment height matches a style Button")
            }

            const main = mainButton().background
            compare(main.topLeftRadius, Theme.controlRadius, "outer corners stay round")
            compare(main.topRightRadius, 0, "the main segment squares its seam corners")
            const chevron = menuButton().background
            compare(chevron.topLeftRadius, 0, "the chevron squares its seam corners")
            compare(chevron.bottomRightRadius, Theme.controlRadius, "outer corners stay round")
        }

        function test_mainButtonRoundOnBothSidesWithoutMenu() {
            compare(mainButton().background.topRightRadius, Theme.controlRadius,
                    "a lone main segment keeps all four corners round")
        }

        // The seam column between the segments is the shared outline.
        function test_seamIsTheButtonBorder() {
            splitButtonId.menu = menuId
            waitForChevronPlacement()
            tryVerify(() => !mainButton().hovered && !menuButton().hovered)

            waitForRendering(splitButtonId)
            const img = grabImage(splitButtonId)
            // grabImage returns device pixels.
            const scale = img.width / splitButtonId.width
            const x = Math.round(menuButton().x * scale)
            const y = Math.round((menuButton().height / 2) * scale)
            compareColor(pixelAt(img, x, y), Theme.buttonBorder, "the seam is the style's button border")
        }

        function test_hoverAndPressFollowTheStyle() {
            splitButtonId.menu = menuId
            waitForChevronPlacement()

            const main = mainButton()
            // The offscreen platform turns hover effects off by default.
            main.hoverEnabled = true
            mouseMove(main, main.width / 2, main.height / 2)
            tryVerify(() => main.hovered, 5000, "the main segment is hovered")
            compareColor(main.background.color, Theme.buttonHover, "hover uses the style's hover surface")
            compareColor(menuButton().background.color, Theme.buttonSurface,
                         "the other segment stays at rest")

            mousePress(main, main.width / 2, main.height / 2)
            tryVerify(() => main.down, 5000, "the main segment is pressed")
            compareColor(main.background.color, Theme.buttonPressed, "press uses the style's pressed surface")
            mouseRelease(main, main.width / 2, main.height / 2)
            compare(clickedSpyId.count, 1, "press and release fires clicked() once")
        }

        function pixelAt(img, x, y) {
            return Qt.rgba(img.red(x, y) / 255, img.green(x, y) / 255,
                           img.blue(x, y) / 255, 1)
        }

        // The focused segment's ring ends at the divider, leaving the other
        // segment's face clear.
        function test_focusRingStaysOnItsSegment() {
            splitButtonId.menu = menuId
            waitForChevronPlacement()

            const main = mainButton()
            main.forceActiveFocus(Qt.TabFocusReason)
            tryVerify(() => main.visualFocus, 5000, "the main segment has keyboard focus")

            waitForRendering(splitButtonId)
            const img = grabImage(splitButtonId)
            const scale = img.width / splitButtonId.width
            const ringOutset = Theme.focusRingOffset + Theme.focusRingWidth
            const x = Math.round((menuButton().x + ringOutset) * scale)
            const y = Math.round((menuButton().height / 2) * scale)
            compareColor(pixelAt(img, x, y), Theme.buttonSurface,
                         "the chevron's face stays clear of the main segment's ring")
            main.focus = false
        }

        function test_disabledDimsAndIgnoresClicks() {
            splitButtonId.menu = menuId
            waitForChevronPlacement()
            splitButtonId.enabled = false

            compare(mainButton().background.opacity, Theme.disabledOpacity,
                    "the disabled main segment dims like a style Button")
            compare(menuButton().background.opacity, Theme.disabledOpacity,
                    "the disabled chevron dims like a style Button")

            mouseClick(mainButton())
            mouseClick(menuButton())
            compare(clickedSpyId.count, 0, "a disabled split button fires nothing")
            verify(!menuId.visible, "a disabled chevron keeps the menu closed")
        }
    }
}
