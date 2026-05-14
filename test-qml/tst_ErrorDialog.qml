import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

Item {
    id: rootId
    width: 600
    height: 400

    ErrorListModel {
        id: modelId
    }

    ErrorDialog {
        id: errorDialogId
        anchors.centerIn: parent
        model: modelId
    }

    TestCase {
        name: "ErrorDialog"
        when: windowShown

        function init() {
            modelId.clear()
            TestHelper.setClipboardText("")
        }

        function cleanup() {
            modelId.clear()
        }

        // Opens the dialog by appending errors and waits for it to become visible.
        function openWithErrors(errors) {
            for (var i = 0; i < errors.length; i++) {
                TestHelper.appendError(modelId, errors[i].message, errors[i].type)
            }
            var dialog = null
            tryVerify(function() {
                dialog = findChild(rootId, "errorDialog")
                return dialog !== null && dialog.visible
            }, 2000, "ErrorDialog should become visible after adding errors")
            return dialog
        }

        // 1 issue → singular title; Copy All button present and enabled.
        function test_singleIssue_titleAndCopyAllEnabled() {
            const dialog = openWithErrors([
                { message: "lonely warning", type: CwError.Warning }
            ])
            compare(dialog.title, "1 issue has occurred")

            const copyAll = findChild(rootId, "errorDialogCopyAllButton")
            verify(copyAll !== null, "Copy All button must be in the dialog footer")
            verify(copyAll.enabled, "Copy All must be enabled when there is at least one issue")
        }

        // Multiple issues → plural title.
        function test_multipleIssues_pluralTitle() {
            const dialog = openWithErrors([
                { message: "one", type: CwError.Warning },
                { message: "two", type: CwError.Fatal },
                { message: "three", type: CwError.Warning }
            ])
            compare(dialog.title, "3 issues has occurred")
        }

        // Clicking Copy All puts the joined, type-prefixed messages on the clipboard.
        function test_copyAll_writesAllMessagesToClipboard() {
            openWithErrors([
                { message: "compass off by 2", type: CwError.Warning },
                { message: "loop did not close", type: CwError.Fatal }
            ])

            const copyAll = findChild(rootId, "errorDialogCopyAllButton")
            verify(copyAll !== null)
            mouseClick(copyAll)

            const expected = "[Warning] compass off by 2\n[Fatal] loop did not close"
            tryVerify(function() { return TestHelper.clipboardText() === expected }, 2000,
                      "Clipboard should contain joined error text. Actual: '"
                      + TestHelper.clipboardText() + "'")
        }

        // The model helper is reachable from QML and matches what the button copies.
        function test_allMessagesAsText_matchesModel() {
            openWithErrors([
                { message: "first", type: CwError.Warning },
                { message: "second", type: CwError.Fatal }
            ])
            compare(modelId.allMessagesAsText(),
                    "[Warning] first\n[Fatal] second")
        }

        // Accepting the dialog (clicking Ok via accept()) clears the model and closes the dialog.
        function test_ok_clearsModelAndClosesDialog() {
            const dialog = openWithErrors([
                { message: "clear me", type: CwError.Warning }
            ])
            dialog.accept()

            tryVerify(function() { return modelId.count === 0 }, 2000,
                      "Model should be cleared when the user accepts the dialog")
            tryVerify(function() {
                const d = findChild(rootId, "errorDialog")
                return d === null || !d.visible
            }, 2000, "Dialog should close once the model is empty")
        }
    }
}
