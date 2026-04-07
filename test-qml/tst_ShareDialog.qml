import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

Item {
    id: rootId
    width: 600
    height: 400

    ShareDialog {
        id: shareDialogId
    }

    TestCase {
        name: "ShareDialog"
        when: windowShown

        function init() {
            shareDialogId.close()
            RootData.newProject()
            tryVerify(function() { return RootData.project.isTemporaryProject }, 5000)
        }

        function cleanup() {
            shareDialogId.close()
        }

        function setupProjectWithRemote(projectName) {
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tempDir + "/" + projectName + ".cwproj"))
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            tryVerify(function() { return RootData.project.repository !== null }, 5000)
            const err = RootData.project.repository.addRemote(
                "origin", Qt.url("https://github.com/user/testrepo"))
            compare(err, "")
            tryVerify(function() {
                return RootData.project.shareLink().toString().length > 0
            }, 5000)
        }

        // ── Project state conditions ───────────────────────────────────────

        // A new (temporary) project has no git repository, so shareLink is empty.
        function test_shareLink_emptyForTemporaryProject() {
            compare(RootData.project.shareLink().toString(), "")
        }

        // A saved git project with no remote also has an empty shareLink, and
        // syncHealth reports noRemote = true.
        function test_shareLink_emptyAndNoRemoteWhenNoRemoteConfigured() {
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tempDir + "/share-no-remote.cwproj"))
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            compare(RootData.project.fileType, Project.GitFileType)

            // Wait for syncHealth to reflect the absence of a remote.
            tryVerify(function() {
                return RootData.project.syncHealth.status.hasRemote === false
            }, 5000)

            compare(RootData.project.shareLink().toString(), "")
        }

        // After adding an origin remote, shareLink returns the expected URL.
        function test_shareLink_generatedAfterAddingRemote() {
            setupProjectWithRemote("share-with-remote")

            const link = RootData.project.shareLink().toString()
            verify(link.startsWith("https://cavewhere.com/open?repo="),
                   "Expected share link to start with https://cavewhere.com/open?repo=, got: " + link)
            verify(link.indexOf("github.com") >= 0,
                   "Expected share link to contain github.com, got: " + link)
        }

        // syncHealth.status.noRemote becomes false after a remote is added.
        function test_noRemoteFalse_afterAddingRemote() {
            setupProjectWithRemote("share-remote-health")

            tryVerify(function() {
                return RootData.project.syncHealth.status.hasRemote === true
            }, 5000)
        }

        // ── Dialog display ─────────────────────────────────────────────────

        // Copy link button is disabled when there is no share link.
        function test_copyLinkButton_disabledWhenNoLink() {
            shareDialogId.open()
            tryVerify(function() { return shareDialogId.visible }, 1000)

            const button = findChild(rootId, "copyLinkButton")
            verify(button !== null, "copyLinkButton not found")
            verify(!button.enabled, "Copy Link should be disabled when shareLink is empty")
        }

        // Copy Link button is enabled when a share link exists.
        function test_copyLinkButton_enabledWhenLinkExists() {
            setupProjectWithRemote("share-copy-btn")

            shareDialogId.open()
            tryVerify(function() { return shareDialogId.visible }, 1000)

            const button = findChild(rootId, "copyLinkButton")
            verify(button !== null, "copyLinkButton not found")
            verify(button.enabled, "Copy Link should be enabled when shareLink exists")
        }

        // The explanatory note label is hidden when there is no remote (replaced by add remote link).
        function test_noteLabel_hiddenWhenNoRemote() {
            shareDialogId.open()
            tryVerify(function() { return shareDialogId.visible }, 1000)

            const label = findChild(rootId, "shareNoteLabel")
            verify(label !== null, "shareNoteLabel not found")
            verify(!label.visible, "Note label should be hidden when no remote")
        }

        // The explanatory note label is visible when a share link exists.
        function test_noteLabel_visibleWhenShareLinkExists() {
            setupProjectWithRemote("share-note-label")

            shareDialogId.open()
            tryVerify(function() { return shareDialogId.visible }, 1000)

            const label = findChild(rootId, "shareNoteLabel")
            verify(label !== null, "shareNoteLabel not found")
            verify(label.visible, "Note label should be visible when share link exists")
            verify(label.text.length > 0)
        }

        // Invite link says "Invite collaborators on GitHub" for GitHub remotes.
        function test_inviteLink_showsGitHubLabel() {
            setupProjectWithRemote("share-invite-github")

            shareDialogId.open()
            tryVerify(function() { return shareDialogId.visible }, 1000)

            const link = findChild(rootId, "inviteCollaboratorsLink")
            verify(link !== null, "inviteCollaboratorsLink not found")
            compare(link.text, "Invite collaborators on GitHub")
            verify(link.visible, "Invite link should be visible")
        }

        // "Add a remote" link is visible when there is no remote.
        function test_addRemoteLink_visibleWhenNoRemote() {
            shareDialogId.open()
            tryVerify(function() { return shareDialogId.visible }, 1000)

            const link = findChild(rootId, "addRemoteLink")
            verify(link !== null, "addRemoteLink not found")
            verify(link.visible, "Add remote link should be visible when no remote")
        }

        // "Add a remote" link is hidden when a share link exists.
        function test_addRemoteLink_hiddenWhenShareLinkExists() {
            setupProjectWithRemote("share-add-remote")

            shareDialogId.open()
            tryVerify(function() { return shareDialogId.visible }, 1000)

            const link = findChild(rootId, "addRemoteLink")
            verify(link !== null, "addRemoteLink not found")
            verify(!link.visible, "Add remote link should be hidden when share link exists")
        }

        // Clicking "Add a remote" closes the dialog and emits setupRemoteRequested.
        function test_addRemoteLink_closesDialogAndEmitsSignal() {
            var signalEmitted = false
            shareDialogId.setupRemoteRequested.connect(function() {
                signalEmitted = true
            })

            shareDialogId.open()
            tryVerify(function() { return shareDialogId.visible }, 1000)

            const link = findChild(rootId, "addRemoteLink")
            verify(link !== null, "addRemoteLink not found")
            mouseClick(link)

            tryVerify(function() { return !shareDialogId.visible }, 1000)
            verify(signalEmitted, "setupRemoteRequested signal should have been emitted")
        }

        // Invite link is hidden when there is no share link.
        function test_inviteLink_hiddenWhenNoShareLink() {
            shareDialogId.open()
            tryVerify(function() { return shareDialogId.visible }, 1000)

            const link = findChild(rootId, "inviteCollaboratorsLink")
            verify(link !== null, "inviteCollaboratorsLink not found")
            verify(!link.visible, "Invite link should be hidden when no share link")
        }
    }
}
