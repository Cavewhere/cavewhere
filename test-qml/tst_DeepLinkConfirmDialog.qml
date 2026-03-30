import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

Item {
    id: rootId
    width: 600
    height: 400

    DeepLinkConfirmDialog {
        id: dialogId
    }

    TestCase {
        name: "DeepLinkConfirmDialog"
        when: windowShown

        function cleanup() {
            dialogId.close()
        }

        // Dialog opens and shows the host / owner/repo from the URL
        function test_open_showsRepoInfo() {
            dialogId.open(Qt.url("https://github.com/user/repo"))
            var dialog = findChild(rootId, "deepLinkConfirmDialog")
            verify(dialog !== null, "deepLinkConfirmDialog not found")
            tryVerify(function() { return dialog.visible }, 1000)

            var repoLabel = findChild(rootId, "deepLinkRepoLabel")
            verify(repoLabel !== null, "deepLinkRepoLabel not found")
            compare(repoLabel.text, "github.com / user/repo")
        }

        // Cancel button closes the dialog without cloning
        function test_cancel_closesDialog() {
            dialogId.open(Qt.url("https://github.com/user/repo"))
            var dialog = findChild(rootId, "deepLinkConfirmDialog")
            verify(dialog !== null)
            tryVerify(function() { return dialog.visible }, 1000)

            var cancelButton = findChild(rootId, "deepLinkCancelButton")
            verify(cancelButton !== null, "deepLinkCancelButton not found")
            mouseClick(cancelButton)

            tryVerify(function() { return !dialog.visible }, 1000)
        }

        // "Clone & Open" button is enabled when no clone is in progress
        function test_cloneButton_enabledWhenIdle() {
            dialogId.open(Qt.url("https://github.com/user/repo"))
            tryVerify(function() {
                var dialog = findChild(rootId, "deepLinkConfirmDialog")
                return dialog !== null && dialog.visible
            }, 1000)

            var cloneButton = findChild(rootId, "deepLinkCloneButton")
            verify(cloneButton !== null, "deepLinkCloneButton not found")
            verify(cloneButton.enabled, "Clone & Open button should be enabled when idle")
        }

        // Account combobox is present
        function test_accountCombo_present() {
            dialogId.open(Qt.url("https://github.com/user/repo"))
            tryVerify(function() {
                var dialog = findChild(rootId, "deepLinkConfirmDialog")
                return dialog !== null && dialog.visible
            }, 1000)

            var combo = findChild(rootId, "deepLinkAccountCombo")
            verify(combo !== null, "deepLinkAccountCombo not found")
            // Without accounts configured, combobox should still be present
            verify(combo.count > 0, "Account combobox should have at least one entry")
        }

        // Auth flow is hidden by default (no auth error, no add-account in progress)
        function test_authFlow_hiddenByDefault() {
            dialogId.open(Qt.url("https://github.com/user/repo"))
            tryVerify(function() {
                var dialog = findChild(rootId, "deepLinkConfirmDialog")
                return dialog !== null && dialog.visible
            }, 1000)

            var authFlow = findChild(rootId, "deepLinkGitHubAuthFlow")
            verify(authFlow !== null, "deepLinkGitHubAuthFlow not found")
            verify(!authFlow.visible, "Auth flow should be hidden by default")
        }

        // Clicking Clone & Open when clone fails keeps the dialog open and shows the error
        function test_cloneError_keepsDialogOpenAndShowsError() {
            dialogId.open(Qt.url("https://github.com/user/repo"))
            var dialog = findChild(rootId, "deepLinkConfirmDialog")
            verify(dialog !== null)
            tryVerify(function() { return dialog.visible }, 1000)

            var cloneButton = findChild(rootId, "deepLinkCloneButton")
            verify(cloneButton !== null)
            mouseClick(cloneButton)

            // The clone will fail (fake URL / no network in test env).
            // Error area must become visible — either from a synchronous
            // destination-path error or from the async network failure.
            var errorArea = findChild(rootId, "remoteCloneErrorArea")
            verify(errorArea !== null, "remoteCloneErrorArea not found")
            tryVerify(function() { return errorArea.visible }, 5000,
                      "Clone error should be shown after failed clone")

            // Dialog must remain open so the user can see the error
            verify(dialog.visible, "Dialog must stay open after clone error")

            // Cancel button must be enabled so the user can dismiss
            var cancelButton = findChild(rootId, "deepLinkCancelButton")
            verify(cancelButton !== null)
            verify(cancelButton.enabled, "Cancel button must be enabled after clone error")
        }

        // Dialog displays different repos correctly on successive opens
        function test_open_updatesRepoLabel() {
            dialogId.open(Qt.url("https://gitlab.com/org/project"))
            var repoLabel = findChild(rootId, "deepLinkRepoLabel")
            verify(repoLabel !== null)
            tryVerify(function() { return repoLabel.text === "gitlab.com / org/project" }, 1000)

            dialogId.close()
            dialogId.open(Qt.url("https://bitbucket.org/team/cave"))
            tryVerify(function() { return repoLabel.text === "bitbucket.org / team/cave" }, 1000)
        }
    }
}
