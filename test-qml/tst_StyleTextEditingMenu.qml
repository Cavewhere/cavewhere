import QtQuick as QQ
import QtQuick.Controls as QC
import QtTest
import cavewherelib

QQ.Item {
    id: rootId
    width: 400
    height: 320

    QQ.Component {
        id: fieldComponent

        QC.TextField {
            width: 200
        }
    }

    TestCase {
        name: "StyleTextEditingMenu"
        when: windowShown

        function createField(text: string, readOnly: bool) : QC.TextField {
            const field = createTemporaryObject(fieldComponent, rootId,
                                                { text: text, readOnly: readOnly })
            verify(field !== null, "the field should be created")
            return field
        }

        // Right-clicks the field and returns its open menu.
        function openMenu(field: QC.TextField) : QC.Menu {
            mouseClick(field, field.width / 2, field.height / 2, Qt.RightButton)
            const menu = field.QC.ContextMenu.menu
            verify(menu !== null, "the field should carry a context menu")
            tryVerify(() => menu.opened, 5000, "the context menu should open")
            return menu
        }

        function menuItem(menu: QC.Menu, text: string) : QC.MenuItem {
            for (let i = 0; i < menu.count; ++i) {
                const item = menu.itemAt(i)
                if (item.text === text) {
                    return item
                }
            }
            fail("the menu should hold " + text)
            return null
        }

        function enabledItemTexts(menu: QC.Menu) : list<string> {
            const texts = []
            for (let i = 0; i < menu.count; ++i) {
                const item = menu.itemAt(i)
                if (item.action && item.enabled) {
                    texts.push(item.text)
                }
            }
            return texts
        }

        function closeMenu(menu: QC.Menu) {
            menu.close()
            tryVerify(() => !menu.visible, 5000, "the context menu should close")
        }

        function test_offersTheEditActions() {
            const field = createField("A42", false)
            const menu = openMenu(field)
            for (const text of ["Undo", "Redo", "Cut", "Copy", "Paste", "Delete", "Select All"]) {
                verify(menuItem(menu, text).action !== null, text + " should carry its action")
            }
            closeMenu(menu)
        }

        function test_comboBoxAndSpinBoxInputsCarryTheMenu() {
            const comboBox = createTemporaryQmlObject(
                        "import QtQuick.Controls as QC; QC.ComboBox { editable: true; model: [\"A1\", \"A2\"] }",
                        rootId)
            const spinBox = createTemporaryQmlObject(
                        "import QtQuick.Controls as QC; QC.SpinBox { y: 60; editable: true }", rootId)
            for (const control of [comboBox, spinBox]) {
                const input = control.contentItem
                const menu = input.QC.ContextMenu.menu
                verify(menu !== null, "the text input inside " + control + " should carry a context menu")
                compare(menu.editor, input, "the menu should edit the inner text input")
                verify(menuItem(menu, "Paste").enabled, "Paste should be enabled in an editable input")
            }
        }

        function test_copyThenPaste() {
            const field = createField("A42", false)
            field.selectAll()
            const menu = openMenu(field)

            menuItem(menu, "Copy").action.trigger()
            field.cursorPosition = field.length
            compare(field.selectedText, "", "moving the cursor should clear the selection")
            menuItem(menu, "Paste").action.trigger()
            compare(field.text, "A42A42", "Paste should insert the copied text at the cursor")
            closeMenu(menu)
        }

        function test_emptySelectionDisablesCutAndCopy() {
            const field = createField("A42", false)
            field.deselect()
            const menu = openMenu(field)
            compare(menuItem(menu, "Cut").enabled, false, "Cut needs a selection")
            compare(menuItem(menu, "Copy").enabled, false, "Copy needs a selection")
            compare(menuItem(menu, "Select All").enabled, true, "Select All needs no selection")
            closeMenu(menu)
        }

        function test_readOnlyOffersCopyAndSelectAll() {
            const field = createField("A42", true)
            field.selectAll()
            const menu = openMenu(field)
            compare(enabledItemTexts(menu), ["Copy", "Select All"],
                    "a read-only field should enable only Copy and Select All")
            closeMenu(menu)
        }
    }
}
