import QtQuick as QQ
import QtQuick.Window
import QtTest
import QtQuick.Controls as QC
import cavewherelib

QQ.Item {
    id: rootId
    width: 400
    height: 320

    QQ.Column {
        anchors.fill: parent
        spacing: 8

        SelectableValue {
            id: fieldA
            objectName: "fieldA"
            width: 160
        }

        SelectableValue {
            id: fieldB
            objectName: "fieldB"
            width: 160
        }

        // A focusable non-field item, standing in for anything that steals focus
        // from a field without being a SelectableValue (e.g. the context menu).
        QQ.Item {
            id: outsider
            width: 160
            height: 24
            activeFocusOnTab: true
        }

        // Editable field used only to read the clipboard back (paste into it).
        QQ.TextInput {
            id: scratch
            objectName: "scratch"
            width: 160
        }
    }

    TestCase {
        id: testCase
        name: "SelectableValue"
        when: windowShown

        function overlayRoot() {
            return rootId.Window.window.contentItem
        }

        function find(objectName) {
            return findChild(overlayRoot(), objectName)
        }

        // Right-clicks the field and waits until its context menu is fully open,
        // returning the Menu.
        function openContextMenu(field) {
            mouseClick(field, 10, field.height / 2, Qt.RightButton)
            let menu = null
            tryVerify(function() {
                menu = find("selectableValueMenu")
                return menu !== null && menu.opened
            })
            return menu
        }

        // The current clipboard text, read by pasting into an editable field.
        function clipboardText() {
            scratch.text = ""
            scratch.forceActiveFocus()
            scratch.paste()
            return scratch.text
        }

        function init() {
            fieldA.text = "123.45"
            fieldB.text = "678.90"
            fieldA.deselect()
            fieldB.deselect()
            scratch.text = ""
            outsider.forceActiveFocus()
            SelectableValueGroup.current = null
        }

        function cleanup() {
            let menu = find("selectableValueMenu")
            if (menu && menu.opened) {
                menu.close()
                tryVerify(function() { return !menu.opened })
            }
        }

        function test_selectAllSelectsEverything() {
            fieldA.forceActiveFocus()
            fieldA.selectAll()
            compare(fieldA.selectedText, "123.45")
        }

        // A focus-out — as when a context menu opens — must not wipe the selection,
        // or Copy would act on nothing.
        function test_selectionSurvivesFocusLoss() {
            fieldA.forceActiveFocus()
            fieldA.selectAll()
            compare(fieldA.selectedText, "123.45")

            outsider.forceActiveFocus()
            verify(!fieldA.activeFocus)
            compare(fieldA.selectedText, "123.45")
        }

        // The reported bug: Select All from the menu selects a field that never held
        // focus (so no focus-out clears it); selecting another field must clear the
        // first, leaving only one highlight.
        function test_selectingAnotherFieldClearsFirst() {
            fieldA.selectAll()
            compare(fieldA.selectedText, "123.45")

            fieldB.forceActiveFocus()
            fieldB.selectAll()
            compare(fieldB.selectedText, "678.90")
            compare(fieldA.selectedText, "")
        }

        // Merely focusing another field (before selecting anything in it) already
        // clears the previous highlight.
        function test_focusingAnotherFieldClearsFirst() {
            fieldA.forceActiveFocus()
            fieldA.selectAll()
            compare(fieldA.selectedText, "123.45")

            fieldB.forceActiveFocus()
            compare(fieldA.selectedText, "")
        }

        // Right-clicking to open the menu leaves an existing selection intact (the
        // TextField base doesn't forward the right press to the cursor logic) and
        // Copy is live. (The offscreen harness can't fire a native press that would
        // clear it on a raw TextInput, so this documents the behavior rather than
        // reproducing that failure.)
        function test_rightClickPreservesSelectionAndEnablesCopy() {
            fieldA.forceActiveFocus()
            fieldA.selectAll()
            compare(fieldA.selectedText, "123.45")

            openContextMenu(fieldA)
            compare(fieldA.selectedText, "123.45")

            let copyItem = find("selectableValueCopyItem")
            verify(copyItem !== null)
            verify(copyItem.enabled)
        }

        // The right button must never create or change a selection: neither a bare
        // right-click nor a right-button drag may select text (only the menu's
        // Select All does). This is the behavior the TextField base guarantees by
        // not forwarding a right press to the cursor logic. (Under offscreen a
        // synthesized press isn't classified "from mouse", so this also can't
        // reproduce the raw-TextInput failure — it guards against a handler ever
        // selecting on right interaction, and documents the intent.)
        function test_rightButtonDoesNotSelect() {
            mouseClick(fieldA, 10, fieldA.height / 2, Qt.RightButton)
            compare(fieldA.selectedText, "")

            let menu = find("selectableValueMenu")
            if (menu && menu.opened) {
                menu.close()
                tryVerify(function() { return !menu.opened })
            }

            mousePress(fieldA, 4, fieldA.height / 2, Qt.RightButton)
            mouseMove(fieldA, fieldA.width - 4, fieldA.height / 2)
            mouseRelease(fieldA, fieldA.width - 4, fieldA.height / 2, Qt.RightButton)
            compare(fieldA.selectedText, "")
        }

        // Double-click to select, right-click to open the menu: the selection stays,
        // Copy is enabled, and choosing it copies without clearing the selection.
        function test_doubleClickThenRightClickCopies() {
            mouseDoubleClickSequence(fieldA, 10, fieldA.height / 2)
            verify(fieldA.selectedText.length > 0)
            let selected = fieldA.selectedText

            openContextMenu(fieldA)
            // Opening the menu didn't clear the selection.
            compare(fieldA.selectedText, selected)

            let copyItem = find("selectableValueCopyItem")
            verify(copyItem !== null)
            verify(copyItem.enabled)

            mouseClick(copyItem)
            // Choosing Copy neither clears the selection nor fails to copy.
            compare(fieldA.selectedText, selected)
            compare(clipboardText(), selected)
        }

        // Right-click, Select All, right-click again: the text stays selected across
        // the second menu, Copy is still enabled, and it copies the whole value.
        // Right-clicking an unselected field to open the menu must not itself
        // create a selection (the whole reason for the TextField base). Then
        // Select All from the menu selects everything.
        function test_rightClickSelectAllThenRightClickCopies() {
            openContextMenu(fieldA)
            compare(fieldA.selectedText, "")

            let selectAllItem = find("selectableValueSelectAllItem")
            verify(selectAllItem !== null)
            verify(selectAllItem.enabled)
            mouseClick(selectAllItem)
            compare(fieldA.selectedText, "123.45")

            openContextMenu(fieldA)
            // Still fully selected under the reopened menu.
            compare(fieldA.selectedText, "123.45")

            let copyItem = find("selectableValueCopyItem")
            verify(copyItem.enabled)
            mouseClick(copyItem)
            compare(fieldA.selectedText, "123.45")
            compare(clipboardText(), "123.45")
        }
    }
}
