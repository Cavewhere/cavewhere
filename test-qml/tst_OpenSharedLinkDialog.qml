import QtQuick
import QtTest
import cavewherelib

Item {
    id: rootId
    width: 600
    height: 400

    OpenSharedLinkDialog {
        id: dialogId
    }

    SignalSpy {
        id: openRepoSpy
        target: RootData.deepLinkHandler
        signalName: "openRepoRequested"
    }

    SignalSpy {
        id: invalidLinkSpy
        target: RootData.deepLinkHandler
        signalName: "invalidLink"
    }

    TestCase {
        name: "OpenSharedLinkDialog"
        when: windowShown

        function cleanup() {
            dialogId.close()
            openRepoSpy.clear()
            invalidLinkSpy.clear()
        }

        function test_open_showsDialog() {
            dialogId.open()
            tryVerify(function() { return dialogId.visible }, 1000)
        }

        function test_emptyField_openButtonDisabled() {
            dialogId.open()
            tryVerify(function() { return dialogId.visible }, 1000)

            var openButton = findChild(rootId, "openSharedLinkOpenButton")
            verify(openButton !== null, "openSharedLinkOpenButton not found")
            verify(!openButton.enabled, "Open button should be disabled when field is empty")
        }

        function test_nonEmptyField_openButtonEnabled() {
            dialogId.open()
            tryVerify(function() { return dialogId.visible }, 1000)

            var linkField = findChild(rootId, "openSharedLinkField")
            verify(linkField !== null, "openSharedLinkField not found")
            linkField.text = "https://cavewhere.com/open?repo=https%3A//github.com/user/repo"

            var openButton = findChild(rootId, "openSharedLinkOpenButton")
            verify(openButton !== null)
            verify(openButton.enabled, "Open button should be enabled when field has text")
        }

        function test_validShareLink_closesDialogAndEmitsOpenRepo() {
            dialogId.open()
            tryVerify(function() { return dialogId.visible }, 1000)

            var linkField = findChild(rootId, "openSharedLinkField")
            verify(linkField !== null)
            linkField.text = "https://cavewhere.com/open?repo=https%3A//github.com/user/repo"

            var openButton = findChild(rootId, "openSharedLinkOpenButton")
            verify(openButton !== null)
            mouseClick(openButton)

            tryVerify(function() { return openRepoSpy.count > 0 }, 1000,
                      "openRepoRequested should fire for valid share link")
            compare(openRepoSpy.signalArguments[0][0],
                    Qt.url("https://github.com/user/repo"))

            tryVerify(function() { return !dialogId.visible }, 1000,
                      "Dialog should close after valid link accepted")
        }

        function test_validCavewhereLink_closesDialogAndEmitsOpenRepo() {
            dialogId.open()
            tryVerify(function() { return dialogId.visible }, 1000)

            var linkField = findChild(rootId, "openSharedLinkField")
            verify(linkField !== null)
            linkField.text = "cavewhere://open?repo=https://github.com/user/repo"

            var openButton = findChild(rootId, "openSharedLinkOpenButton")
            verify(openButton !== null)
            mouseClick(openButton)

            tryVerify(function() { return openRepoSpy.count > 0 }, 1000,
                      "openRepoRequested should fire for cavewhere:// link")

            tryVerify(function() { return !dialogId.visible }, 1000,
                      "Dialog should close after valid link accepted")
        }

        function test_invalidLink_showsError() {
            dialogId.open()
            tryVerify(function() { return dialogId.visible }, 1000)

            var linkField = findChild(rootId, "openSharedLinkField")
            verify(linkField !== null)
            linkField.text = "notaurl"

            var openButton = findChild(rootId, "openSharedLinkOpenButton")
            verify(openButton !== null)
            mouseClick(openButton)

            var errorLabel = findChild(rootId, "openSharedLinkError")
            verify(errorLabel !== null, "openSharedLinkError not found")
            tryVerify(function() { return errorLabel.visible && errorLabel.text.length > 0 }, 1000,
                      "Error label should be visible with error text")

            verify(dialogId.visible, "Dialog should remain open after invalid link")
        }

        function test_cancel_closesDialog() {
            dialogId.open()
            tryVerify(function() { return dialogId.visible }, 1000)

            var cancelButton = findChild(rootId, "openSharedLinkCancelButton")
            verify(cancelButton !== null, "openSharedLinkCancelButton not found")
            mouseClick(cancelButton)

            tryVerify(function() { return !dialogId.visible }, 1000,
                      "Dialog should close on cancel")
        }

        function test_fieldClearedOnReopen() {
            dialogId.open()
            tryVerify(function() { return dialogId.visible }, 1000)

            var linkField = findChild(rootId, "openSharedLinkField")
            verify(linkField !== null)
            linkField.text = "some text"
            dialogId.close()

            dialogId.open()
            tryVerify(function() { return dialogId.visible }, 1000)
            compare(linkField.text, "", "Field should be cleared on reopen")

            var errorLabel = findChild(rootId, "openSharedLinkError")
            verify(errorLabel !== null)
            verify(!errorLabel.visible, "Error label should be hidden on reopen")
        }
    }
}
