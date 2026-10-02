import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    TestCase {
        name: "StyleGalleryPage"
        when: windowShown

        function cardNames() {
            return ["cardButtons", "cardToggles", "cardIndicators", "cardFields", "cardProgress",
                    "cardMenus", "cardContainers", "cardData", "cardDialogs", "cardCaveWhere"]
        }

        function modes() {
            return [{ name: "light", scheme: AppearanceSettings.Light, dark: false },
                    { name: "dark", scheme: AppearanceSettings.Dark, dark: true }]
        }

        function cleanup() {
            const page = RootData.pageView.currentPageItem
            if (page !== null && page.objectName === "styleGalleryPage") {
                page.closePopups()
            }
            RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.System
            const entries = RootData.settings.fontSettings.fontEntries
            RootData.settings.fontSettings.fontFamily = entries[0].family
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function gotoGallery() {
            RootData.pageSelectionModel.gotoPageByName(null, "Style Gallery")
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "styleGalleryPage",
                      5000, "should land on styleGalleryPage")
            return RootData.pageView.currentPageItem
        }

        // The test window is a plain view, where an existing menu or dialog keeps
        // the palette it was created with; taking its parent again makes it read
        // the window's palette. The application's window type updates popups live.
        function adoptWindowPalette(page, popupName) {
            const popup = findChild(page, popupName)
            verify(popup !== null, popupName + " should exist")
            const popupParent = popup.parent
            popup.parent = null
            popup.parent = popupParent
        }

        function setMode(mode) {
            RootData.settings.appearanceSettings.colorScheme = mode.scheme
            const page = gotoGallery()
            tryVerify(() => Theme.dark === mode.dark, 5000, "Theme.dark should follow " + mode.name)
            tryVerify(() => Qt.colorEqual(page.palette.windowText, Theme.text)
                            && Qt.colorEqual(page.palette.base, Theme.fieldSurface),
                      5000, "the Theme palette should reach the page in " + mode.name)
            for (const popupName of ["galleryMenu", "galleryDialog"]) {
                adoptWindowPalette(page, popupName)
            }
            return page
        }

        function test_loadsInLightAndDark() {
            // Painter items such as TaskProgressRing's ring report a missing QRhi
            // on the offscreen platform; every other warning fails the test.
            failOnWarning(/^(?!No QRhi found for window).*/)

            for (const mode of modes()) {
                const page = setMode(mode)
                for (const cardName of cardNames()) {
                    verify(findChild(page, cardName) !== null, cardName + " should exist in " + mode.name)
                }
            }
        }

        function findButton(item, text) {
            for (let i = 0; i < item.children.length; ++i) {
                const child = item.children[i]
                if (child.highlighted !== undefined && child.text === text) {
                    return child
                }
                const found = findButton(child, text)
                if (found !== null) {
                    return found
                }
            }
            return null
        }

        function test_styleIsCaveWhere() {
            const page = gotoGallery()
            const card = findChild(page, "cardButtons")
            verify(card !== null, "cardButtons should exist")

            const button = findButton(card, "Export")
            verify(button !== null, "cardButtons should hold the plain Export button")
            verify(button.background !== null, "the button should have a background")
            compare(button.background.radius, Theme.controlRadius,
                    "the button background should come from CaveWhereStyle")
        }

        function test_popupsOpen() {
            const page = gotoGallery()

            const menu = findChild(page, "galleryMenu")
            verify(menu !== null, "galleryMenu should exist")
            page.openMenu()
            tryVerify(() => menu.visible, 5000, "the menu should open")
            page.closePopups()
            tryVerify(() => !menu.visible, 5000, "the menu should close")

            const dialog = findChild(page, "galleryDialog")
            verify(dialog !== null, "galleryDialog should exist")
            page.openDialog()
            tryVerify(() => dialog.visible, 5000, "the dialog should open")
            page.closePopups()
            tryVerify(() => !dialog.visible, 5000, "the dialog should close")
        }

        function test_screenshots() {
            if (Qt.platform.pluginName === "offscreen") {
                skip("needs a GPU-backed platform")
            }

            for (const mode of modes()) {
                const page = setMode(mode)
                const scrollView = findChild(page, "styleGalleryScrollView")
                verify(scrollView !== null, "styleGalleryScrollView should exist")
                const flickable = scrollView.contentItem

                let step = 0
                for (let y = 0; y < Math.max(1, flickable.contentHeight); y += flickable.height) {
                    flickable.contentY = Math.min(y, Math.max(0, flickable.contentHeight - flickable.height))
                    tryCompare(flickable, "moving", false)
                    const path = WindowGrabber.grabToFile(page, "style-gallery-" + mode.name + "-" + step)
                    verify(path.length > 0, "grabToFile wrote step " + step + " in " + mode.name)
                    step++
                }
                flickable.contentY = 0

                const menu = findChild(page, "galleryMenu")
                page.openMenu()
                tryVerify(() => menu.opened, 5000, "the menu should finish opening")
                verify(WindowGrabber.grabToFile(page, "style-gallery-" + mode.name + "-menu").length > 0)
                page.closePopups()
                tryVerify(() => !menu.visible)

                const dialog = findChild(page, "galleryDialog")
                page.openDialog()
                tryVerify(() => dialog.opened, 5000, "the dialog should finish opening")
                verify(WindowGrabber.grabToFile(page, "style-gallery-" + mode.name + "-dialog").length > 0)
                page.closePopups()
                tryVerify(() => !dialog.visible)
            }
        }
    }
}
