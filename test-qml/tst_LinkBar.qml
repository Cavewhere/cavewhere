import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    TestCase {
        name: "LinkBar"
        when: windowShown

        function linkBar() {
            return findChild(rootId.mainWindow, "linkBar")
        }

        function breadcrumb() {
            return findChild(rootId.mainWindow, "linkBarBreadcrumb")
        }

        function addressField() {
            return findChild(rootId.mainWindow, "linkBarAddressField")
        }

        function findMenu() {
            return findChild(rootId.QQ.Window.window.contentItem, "linkBarMenu")
        }

        // Right-clicks an empty spot of the breadcrumb, past the last chip, and
        // waits until the menu is fully open.
        function openMenu() {
            let crumb = breadcrumb()
            verify(crumb !== null)
            mouseClick(crumb, crumb.width - 10, crumb.height / 2, Qt.RightButton)
            let menu = null
            tryVerify(function() {
                menu = findMenu()
                return menu !== null && menu.opened
            })
            return menu
        }

        function findButtonWithText(item, text) {
            if (item === null || item === undefined) {
                return null
            }
            if (item.text === text && item.clicked !== undefined) {
                return item
            }
            const children = item.children
            for (let i = 0; i < children.length; ++i) {
                const found = findButtonWithText(children[i], text)
                if (found !== null) {
                    return found
                }
            }
            return null
        }

        function init() {
            RootData.pageSelectionModel.gotoPageByName(null, "View")
            tryCompare(RootData.pageSelectionModel, "currentPageAddress", "View")
        }

        function cleanup() {
            let menu = findMenu()
            if (menu && menu.opened) {
                menu.close()
                tryVerify(function() { return !menu.opened })
            }
            linkBar().editingAddress = false
        }

        function test_menuHoldsEditAndCopy() {
            let menu = openMenu()
            compare(menu.count, 2)
            compare(menu.itemAt(0).text, "Edit")
            compare(menu.itemAt(0).objectName, "linkBarEditItem")
            compare(menu.itemAt(1).text, "Copy")
            compare(menu.itemAt(1).objectName, "linkBarCopyItem")
        }

        function test_editShowsAddressAndEscapeDiscards() {
            RootData.pageSelectionModel.gotoPageByName(null, "Data")
            tryVerify(function() { return RootData.pageSelectionModel.currentPageAddress !== "View" })
            const address = RootData.pageSelectionModel.currentPageAddress

            let field = addressField()
            let listView = findChild(rootId.mainWindow, "linkBarListView")
            verify(!field.visible)
            verify(listView.visible)

            let menu = openMenu()
            menu.itemAt(0).triggered()
            tryVerify(function() { return field.visible && field.activeFocus })
            verify(!listView.visible)
            compare(field.text, address)
            compare(field.selectedText, address)

            keyClick(Qt.Key_X)
            compare(field.text, "x")

            keyClick(Qt.Key_Escape)
            tryVerify(function() { return !field.visible })
            verify(listView.visible)
            compare(RootData.pageSelectionModel.currentPageAddress, address)
            compare(field.text, address)
        }

        function test_enterAppliesTypedAddress() {
            RootData.pageSelectionModel.gotoPageByName(null, "Data")
            tryVerify(function() { return RootData.pageSelectionModel.currentPageAddress !== "View" })

            let field = addressField()
            let menu = openMenu()
            menu.itemAt(0).triggered()
            tryVerify(function() { return field.visible && field.activeFocus })

            field.text = "View"
            keyClick(Qt.Key_Return)
            tryCompare(RootData.pageSelectionModel, "currentPageAddress", "View")
            tryVerify(function() { return !field.visible })
        }

        function test_copyPutsAddressOnClipboard() {
            const sentinel = "linkBar clipboard sentinel"
            TestHelper.setClipboardText(sentinel)
            const clipboardWorks = TestHelper.clipboardText() === sentinel

            let menu = openMenu()
            menu.itemAt(1).triggered()
            tryVerify(function() { return !menu.opened })

            // Some headless platforms keep no clipboard; check the text only
            // where a write reads back.
            if (clipboardWorks) {
                const address = RootData.pageSelectionModel.currentPageAddress
                tryVerify(function() { return TestHelper.clipboardText() === address })
            }
        }

        function test_breadcrumbHasNoBoxOrEllipsisButton() {
            let crumb = breadcrumb()
            verify(crumb !== null)
            compare(crumb.border, undefined, "the breadcrumb area draws no border")
            compare(crumb.color, undefined, "the breadcrumb area draws no fill")
            compare(findButtonWithText(linkBar(), "..."), null)
        }

        function test_rightButtonsShareTheGap() {
            let bar = linkBar()
            tryVerify(function() { return bar.layoutSize >= Theme.LayoutSize.Wide })
            let sync = findChild(bar, "syncButton")
            let discord = findChild(bar, "discordButton")
            verify(sync !== null && discord !== null)
            tryVerify(function() { return discord.visible })
            compare(sync.x + sync.width + Theme.linkBarButtonSpacing, discord.x)
            let crumb = breadcrumb()
            let chip = findChild(bar, "taskStatusChip")
            let next = chip.visible ? chip : sync
            verify(next.x - (crumb.x + crumb.width) >= Theme.linkBarButtonSpacing)
        }
    }
}
