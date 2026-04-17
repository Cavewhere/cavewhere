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

    SignalSpy {
        id: openRequestedSpy
        target: dialogId
        signalName: "openRequested"
    }

    TestCase {
        name: "DeepLinkConfirmDialog"
        when: windowShown

        function cleanup() {
            dialogId.close()
            openRequestedSpy.clear()
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

        // Raw HTTPS git clone URL (not a cavewhere deep link) opens and
        // displays host/path. Cloning a fake host fails via the network so
        // the cloner reports its error — the dialog stays open.
        function test_open_httpsCloneUrl_showsRepoInfo() {
            dialogId.open("https://github.com/Cavewhere/PhakeCave3000.git")
            var dialog = findChild(rootId, "deepLinkConfirmDialog")
            verify(dialog !== null)
            tryVerify(function() { return dialog.visible }, 1000)

            var repoLabel = findChild(rootId, "deepLinkRepoLabel")
            verify(repoLabel !== null)
            compare(repoLabel.text, "github.com / Cavewhere/PhakeCave3000.git")

            // Account picker is visible for HTTPS URLs.
            var accountRow = findChild(rootId, "deepLinkAccountRow")
            verify(accountRow !== null, "deepLinkAccountRow not found")
            verify(accountRow.visible, "Account row should be visible for HTTPS clone URL")
        }

        // SSH clone URL authenticates with a local SSH key — the dialog must
        // hide the Account row and the GitHub auth flow. Clone attempt still
        // proceeds (and fails in the test env with whatever message the
        // cloner reports); the dialog stays open either way.
        function test_open_sshCloneUrl_hidesAccountAndAuth() {
            dialogId.open("git@github.com:Cavewhere/PhakeCave3000.git")
            var dialog = findChild(rootId, "deepLinkConfirmDialog")
            verify(dialog !== null)
            tryVerify(function() { return dialog.visible }, 1000)

            // Account picker and auth flow hidden for SSH.
            var accountRow = findChild(rootId, "deepLinkAccountRow")
            verify(accountRow !== null)
            verify(!accountRow.visible, "Account row should be hidden for SSH clone URL")

            var authFlow = findChild(rootId, "deepLinkGitHubAuthFlow")
            verify(authFlow !== null)
            verify(!authFlow.visible, "Auth flow should be hidden for SSH clone URL")

            // Repo label falls back to the raw text (URL() parsing fails on SSH shape).
            var repoLabel = findChild(rootId, "deepLinkRepoLabel")
            verify(repoLabel !== null)
            compare(repoLabel.text, "git@github.com:Cavewhere/PhakeCave3000.git")
        }

        // HTTP (non-HTTPS) clone URLs reach the next screen. The cloner is
        // given the raw URL; it does not pre-validate the scheme, so the
        // clone attempt fails and the dialog surfaces the error.
        function test_httpCloneUrl_reachesDialogAndSurfacesError() {
            dialogId.open("http://github.com/Cavewhere/PhakeCave3000.git")
            var dialog = findChild(rootId, "deepLinkConfirmDialog")
            verify(dialog !== null)
            tryVerify(function() { return dialog.visible }, 1000)

            var repoLabel = findChild(rootId, "deepLinkRepoLabel")
            verify(repoLabel !== null)
            compare(repoLabel.text, "github.com / Cavewhere/PhakeCave3000.git")

            var cloneButton = findChild(rootId, "deepLinkCloneButton")
            verify(cloneButton !== null)
            mouseClick(cloneButton)

            var errorArea = findChild(rootId, "remoteCloneErrorArea")
            verify(errorArea !== null)
            tryVerify(function() { return errorArea.visible }, 10000,
                      "HTTP clone should surface a cloner error")
            verify(dialog.visible, "Dialog must stay open after clone error")
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

        // Reopening the dialog must not carry over status/error text from a
        // previous clone attempt — otherwise the user sees stale messages
        // like "Clone complete." before they've even clicked Clone.
        function test_reopen_clearsPreviousCloneState() {
            var fixture = TestHelper.createLocalBareRemoteForCloneTest()
            compare(fixture.errorMessage, "")
            RootData.recentProjectModel.defaultRepositoryDir = TestHelper.toLocalUrl(fixture.cloneParentPath)

            dialogId.open(Qt.url(fixture.remoteUrl))
            var dialog = findChild(rootId, "deepLinkConfirmDialog")
            verify(dialog !== null)
            tryVerify(function() { return dialog.visible }, 1000)

            var cloneButton = findChild(rootId, "deepLinkCloneButton")
            verify(cloneButton !== null)
            mouseClick(cloneButton)

            // Wait for the first clone to reach its terminal status message.
            var statusText = findChild(rootId, "remoteCloneStatusText")
            verify(statusText !== null, "remoteCloneStatusText not found")
            tryVerify(function() { return statusText.text === "Clone complete." },
                      20000, "First clone should finish with 'Clone complete.'")

            dialogId.close()
            tryVerify(function() { return !dialog.visible }, 1000)

            // Reopen for a different URL — prior status must be cleared.
            dialogId.open("https://github.com/Cavewhere/PhakeCave3000.git")
            tryVerify(function() { return dialog.visible }, 1000)
            verify(statusText.text.length === 0,
                   "Reopened dialog still shows stale status: '" + statusText.text + "'")

            var errorArea = findChild(rootId, "remoteCloneErrorArea")
            verify(errorArea !== null)
            verify(!errorArea.visible,
                   "Reopened dialog still shows stale error area")
        }

        // After a successful clone, openRequested is emitted with the project file path.
        function test_clone_emitsOpenRequested() {
            var fixture = TestHelper.createLocalBareRemoteForCloneTest()
            compare(fixture.errorMessage, "")

            RootData.recentProjectModel.defaultRepositoryDir = TestHelper.toLocalUrl(fixture.cloneParentPath)
            dialogId.open(Qt.url(fixture.remoteUrl))
            tryVerify(function() {
                var dialog = findChild(rootId, "deepLinkConfirmDialog")
                return dialog !== null && dialog.visible
            }, 1000)

            var cloneButton = findChild(rootId, "deepLinkCloneButton")
            verify(cloneButton !== null)
            mouseClick(cloneButton)

            tryVerify(function() { return openRequestedSpy.count > 0 }, 20000,
                      "openRequested should fire after clone succeeds")
            verify(openRequestedSpy.signalArguments[0][0].indexOf(fixture.expectedClonePath) >= 0,
                   "openRequested should carry the cloned project file path")
        }
    }
}
