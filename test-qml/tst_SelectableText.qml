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

        // First in the tree, so find() reaches its menu before any other.
        SelectableText {
            id: textField
            objectName: "textField"
            width: 160
        }

        SelectableValue {
            id: valueField
            objectName: "valueField"
            width: 160
        }

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
        name: "SelectableText"
        when: windowShown

        readonly property string coordinate: "37.123456° N, 85.654321° W"

        function find(objectName) {
            return findChild(rootId.Window.window.contentItem, objectName)
        }

        function openContextMenu(field) {
            mouseClick(field, 10, 6, Qt.RightButton)
            let menu = null
            tryVerify(function() {
                menu = find("selectableValueMenu")
                return menu !== null && menu.opened
            })
            return menu
        }

        function clipboardText() {
            scratch.text = ""
            scratch.forceActiveFocus()
            scratch.paste()
            return scratch.text
        }

        function init() {
            textField.width = 160
            textField.text = testCase.coordinate
            valueField.text = "678.90"
            textField.deselect()
            valueField.deselect()
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

        // A coordinate wider than its row folds onto a second line, and the
        // field still asks for its full one-line width, so a row that widens
        // again can give it back.
        function test_aLongValueWrapsAndKeepsItsNaturalWidth() {
            const oneLineHeight = textField.implicitHeight
            const naturalWidth = textField.implicitWidth

            textField.width = naturalWidth / 2
            tryVerify(() => textField.implicitHeight > oneLineHeight, 1000,
                      "the value folds onto more lines when the row narrows")
            compare(textField.implicitWidth, naturalWidth)
        }

        function test_rightClickSelectAllThenCopy() {
            openContextMenu(textField)
            compare(textField.selectedText, "")

            mouseClick(find("selectableValueSelectAllItem"))
            compare(textField.selectedText, testCase.coordinate)

            openContextMenu(textField)
            compare(textField.selectedText, testCase.coordinate)

            const copyItem = find("selectableValueCopyItem")
            verify(copyItem.enabled)
            mouseClick(copyItem)
            compare(textField.selectedText, testCase.coordinate)
            compare(clipboardText(), testCase.coordinate)
        }

        // One highlight across both kinds of field: selecting either clears
        // the other.
        function test_selectingEitherKindClearsTheOther() {
            textField.forceActiveFocus()
            textField.selectAll()
            compare(textField.selectedText, testCase.coordinate)

            valueField.forceActiveFocus()
            valueField.selectAll()
            compare(textField.selectedText, "")

            textField.forceActiveFocus()
            compare(valueField.selectedText, "")
        }
    }
}
