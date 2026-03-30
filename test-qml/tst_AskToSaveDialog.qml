import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

Item {
    id: rootId
    width: 400
    height: 300

    property bool afterSaveCalled: false

    SaveAsDialog {
        id: saveAsDialogId
    }

    AskToSaveDialog {
        id: askToSaveDialogId
        saveAsDialog: saveAsDialogId
        taskName: "closing"
        anchors.centerIn: parent
    }

    CWTestCase {
        name: "AskToSaveDialog"
        when: windowShown

        function init() {
            RootData.newProject();
            RootData.account.name = "Test User";
            RootData.account.email = "test@example.com";
            rootId.afterSaveCalled = false;
            askToSaveDialogId.afterSaveFunc = function() { rootId.afterSaveCalled = true; };
            askToSaveDialogId.closeDialog();
            tryVerify(function() { return RootData.project.isTemporaryProject; });
        }

        // Saves the current project as a .cwproj directory under a fresh temp dir.
        function saveAsDirectory(name) {
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            saveAsDialogId.bundleFormat = false;
            saveAsDialogId.destinationFolder = tempDir;
            saveAsDialogId._pendingName = name;
            saveAsDialogId.bundleFilePath = "";
            saveAsDialogId.accepted();
            TestHelper.waitForProjectSaveToFinish(RootData.project);
            tryVerify(function() { return !RootData.project.isTemporaryProject; });
        }

        // New empty project: afterSaveFunc is called immediately without showing a dialog
        function test_newEmptyProject_skipsDialog() {
            verify(RootData.project.isNewEmptyProject());

            askToSaveDialogId.askToSave();

            verify(rootId.afterSaveCalled,
                   "afterSaveFunc should be called immediately for a new empty project");
            verify(askToSaveDialogId._dialog === null
                   || !askToSaveDialogId._dialog.askToSaveDialog.visible,
                   "dialog should not be shown for a new empty project");
        }

        // Saved, unmodified project: afterSaveFunc is called immediately without a dialog.
        // After saveAsDirectory, ensureLfsAttributes() creates .gitattributes after the
        // initial commit, leaving them untracked. A second save() commits those files,
        // making the project truly clean.
        function test_unmodifiedProject_skipsDialog() {
            saveAsDirectory("askToSave-unmodified");
            // Commit any untracked initialization files (e.g., .gitattributes for LFS)
            RootData.project.save();
            TestHelper.waitForProjectSaveToFinish(RootData.project);
            tryVerify(function() { return !RootData.project.isModified(); }, 5000);

            askToSaveDialogId.askToSave();

            verify(rootId.afterSaveCalled,
                   "afterSaveFunc should be called immediately for an unmodified project");
        }

        // Modified project: dialog is shown and afterSaveFunc is NOT yet called
        function test_modifiedProject_showsDialog() {
            TestHelper.loadProjectFromFile(RootData.project,
                                           "://datasets/test_cwProject/Phake Cave 3000.cw");
            tryVerify(function() { return RootData.region.caveCount > 0; });
            saveAsDirectory("askToSave-modified");

            RootData.region.cave(0).name = "Modified Cave";
            tryVerify(function() { return RootData.project.isModified(); });

            askToSaveDialogId.askToSave();

            verify(!rootId.afterSaveCalled,
                   "afterSaveFunc should NOT be called before user responds to dialog");
            verify(askToSaveDialogId._dialog !== null);
            verify(askToSaveDialogId._dialog.askToSaveDialog.visible,
                   "dialog should be visible for a modified project");
        }

        // Ctrl+S on a temporary project (canSaveDirectly is false) opens SaveAsDialog directly.
        // Saving as a .cw bundle: isTemporaryProject becomes false and filename is set
        // so the title bar shows the real path instead of "New File".
        // init() already guarantees isTemporaryProject = true via tryVerify.
        function test_ctrlS_temporaryToBundle_filenameUpdatesForTitleBar() {
            verify(!RootData.project.canSaveDirectly,
                   "canSaveDirectly must be false for a fresh temporary project");

            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const name = "ctrl-s-temp-bundle";
            const expectedPath = tempDir + "/" + name + ".cw";

            saveAsDialogId.bundleFormat = true;
            saveAsDialogId.destinationFolder = tempDir;
            saveAsDialogId._pendingName = name;
            saveAsDialogId.bundleFilePath = expectedPath;
            saveAsDialogId.accepted();
            TestHelper.waitForProjectSaveToFinish(RootData.project);
            tryVerify(function() { return TestHelper.fileExists(TestHelper.toLocalUrl(expectedPath)); });

            verify(!RootData.project.isTemporaryProject,
                   "project must not be temporary after saving as bundle");
            // filename drives the title bar: isTemporaryProject ? "New File" : filename
            compare(RootData.project.filename, expectedPath);
        }

        // Saving a bundle project as a .cwproj directory: filename must reflect the new
        // .cwproj path, not the old .cw bundle path.
        // (Regression: filenameChanged fired while LoadedFromBundledArchive was still true,
        // leaving the title bar stuck on the old .cw path.)
        function test_saveAsDirectory_afterBundle_filenameUpdatesForTitleBar() {
            // First save the temporary project as a .cw bundle.
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const bundleName = "bundle-then-dir-src";
            const bundlePath = tempDir + "/" + bundleName + ".cw";

            saveAsDialogId.bundleFormat = true;
            saveAsDialogId.destinationFolder = tempDir;
            saveAsDialogId._pendingName = bundleName;
            saveAsDialogId.bundleFilePath = bundlePath;
            saveAsDialogId.accepted();
            TestHelper.waitForProjectSaveToFinish(RootData.project);
            tryVerify(function() { return !RootData.project.isTemporaryProject; });

            // Now save the same project as a directory.
            const dirName = "bundle-then-dir-dest";
            const expectedPath = tempDir + "/" + dirName + "/" + dirName + ".cwproj";

            saveAsDialogId.bundleFormat = false;
            saveAsDialogId.destinationFolder = tempDir;
            saveAsDialogId._pendingName = dirName;
            saveAsDialogId.bundleFilePath = "";
            saveAsDialogId.accepted();
            TestHelper.waitForProjectSaveToFinish(RootData.project);
            tryVerify(function() { return TestHelper.fileExists(TestHelper.toLocalUrl(expectedPath)); });

            // filename must now reflect the .cwproj path, not the old .cw path.
            compare(RootData.project.filename, expectedPath);
        }

        // AskToSaveDialog "Save temporary project" flow (e.g. quit with unsaved temp project):
        // accepting the dialog opens SaveAsDialog; completing that save transitions the project
        // from temporary to permanent and updates filename for the title bar.
        // A cave is added to make the project non-empty so askToSave() shows the dialog.
        function test_askToSave_temporaryProject_acceptSaveBundle_filenameUpdates() {
            // Make the project non-empty so isNewEmptyProject() returns false,
            // which causes askToSave() to show the dialog instead of skipping it.
            RootData.region.addCave();
            tryVerify(function() { return !RootData.project.isNewEmptyProject(); });
            tryVerify(function() { return RootData.project.isModified(); });

            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const name = "ask-save-temp-bundle";
            const expectedPath = tempDir + "/" + name + ".cw";

            askToSaveDialogId.askToSave();
            verify(askToSaveDialogId._dialog !== null);
            verify(askToSaveDialogId._dialog.askToSaveDialog.visible,
                   "AskToSaveDialog should show for a modified temporary project");
            verify(askToSaveDialogId._dialog.isTemporaryProject,
                   "dialog should be in temporary-project mode");

            // Accepting the AskToSaveDialog calls handleTemporarySaveRequest() → saveAsDialog.open()
            // which fires onAboutToShow and resets the dialog properties.
            // Re-configure after open() so our test path wins.
            askToSaveDialogId._dialog.askToSaveDialog.accepted();
            saveAsDialogId.bundleFormat = true;
            saveAsDialogId.destinationFolder = tempDir;
            saveAsDialogId._pendingName = name;
            saveAsDialogId.bundleFilePath = expectedPath;
            saveAsDialogId.accepted();

            TestHelper.waitForProjectSaveToFinish(RootData.project);
            tryVerify(function() { return !RootData.project.isTemporaryProject; });

            verify(!RootData.project.isTemporaryProject);
            compare(RootData.project.filename, expectedPath);
            verify(rootId.afterSaveCalled, "afterSaveFunc should be called after save completes");
        }

        // Temporary modified project: Save button must be within the dialog's geometry so
        // the user can click it. Regression: two AcceptRole buttons (Save + Close anyway) in
        // the same DialogButtonBox caused Qt Quick Controls to overflow the extra button
        // outside the popup rect, where it was invisible but still responded to accepted().
        function test_temporaryModifiedProject_saveButtonVisible() {
            RootData.region.addCave();
            tryVerify(function() { return !RootData.project.isNewEmptyProject(); });
            tryVerify(function() { return RootData.project.isModified(); });
            verify(RootData.project.isTemporaryProject,
                   "project must still be temporary");

            askToSaveDialogId.askToSave();

            verify(askToSaveDialogId._dialog !== null);
            verify(askToSaveDialogId._dialog.askToSaveDialog.visible,
                   "dialog should be visible for a modified temporary project");
            verify(askToSaveDialogId._dialog.isTemporaryProject,
                   "dialog should be in temporary-project mode");

            // Accepting a temporary project dialog must open SaveAsDialog, not close silently.
            askToSaveDialogId._dialog.askToSaveDialog.accepted();
            tryVerify(function() { return saveAsDialogId.visible; }, 2000,
                      "SaveAsDialog should open when Save is accepted on a temporary project");
        }

        // Temporary project: cancelling the SaveAsDialog should dismiss the AskToSaveDialog too.
        // There is no meaningful state to return to once the user has declined to save.
        function test_askToSave_temporaryProject_cancelSaveDialog_closesBoth() {
            RootData.region.addCave();
            tryVerify(function() { return !RootData.project.isNewEmptyProject(); });
            tryVerify(function() { return RootData.project.isModified(); });

            askToSaveDialogId.askToSave();
            verify(askToSaveDialogId._dialog !== null);
            verify(askToSaveDialogId._dialog.askToSaveDialog.visible,
                   "AskToSaveDialog should be visible for a modified temporary project");

            // Accept the AskToSaveDialog "Save" button to open SaveAsDialog
            askToSaveDialogId._dialog.askToSaveDialog.accepted();
            tryVerify(function() { return saveAsDialogId.visible; }, 2000,
                      "SaveAsDialog should open after accepting on a temporary project");

            // Cancel the SaveAsDialog — AskToSaveDialog should close too
            saveAsDialogId.rejected();

            tryVerify(function() {
                return askToSaveDialogId._dialog === null
                    || !askToSaveDialogId._dialog.askToSaveDialog.visible;
            }, 2000, "AskToSaveDialog should close when SaveAsDialog is cancelled");
            verify(!rootId.afterSaveCalled,
                   "afterSaveFunc must not be called when the user cancels");
        }

        // Regression: saving a temporary project as .cwproj must create a git commit.
        // Without a commit all files are untracked; discardChanges() calls cleanUntracked()
        // which deletes every untracked file — erasing the entire project from disk.
        function test_saveAsDirectory_fromTemp_commitsData_survivesDiscard() {
            RootData.region.addCave();
            RootData.region.cave(0).name = "Committed Cave";
            tryVerify(function() { return !RootData.project.isNewEmptyProject(); });

            saveAsDirectory("discard-data-loss");
            const savedPath = RootData.project.filename;

            // A commit MUST exist. Without it cleanUntracked() nukes the whole project.
            verify(TestHelper.projectHeadCommitOid(RootData.project) !== "",
                   "saveAs to .cwproj must create a git commit");

            // Simulate reopening and adding unsaved changes
            TestHelper.loadProjectFromPath(RootData.project, savedPath);
            tryVerify(function() { return RootData.region.caveCount > 0; });
            compare(RootData.region.cave(0).name, "Committed Cave");

            RootData.region.addCave();
            RootData.region.cave(1).name = "Unsaved Cave";
            tryVerify(function() { return RootData.project.isModified(); });
            TestHelper.waitForProjectSaveToFinish(RootData.project);

            // Discard the unsaved changes
            RootData.project.discardChanges();
            tryVerify(function() {
                return TestHelper.projectModifiedFileCount(RootData.project) === 0;
            }, 5000, "git working directory should be clean after discardChanges()");

            // Reload and confirm committed data survived
            TestHelper.loadProjectFromPath(RootData.project, savedPath);
            tryVerify(function() { return RootData.region.caveCount > 0; });
            compare(RootData.region.cave(0).name, "Committed Cave",
                    "Committed cave data must survive discardChanges()");
        }

        // Saving a temporary project via SaveAsDialog must add it to the recent projects model.
        function test_saveAsDirectory_fromTemp_addsToRecentProjects() {
            const before = RootData.recentProjectModel.rowCount();

            saveAsDirectory("recent-project-dir");

            tryVerify(function() {
                return RootData.recentProjectModel.rowCount() === before + 1;
            }, 5000, "recent project model should gain one entry after saveAs to directory");
        }

        // Saving a temporary project as a bundle (.cw) must also add it to the recent projects model.
        function test_saveAsBundle_fromTemp_addsToRecentProjects() {
            const before = RootData.recentProjectModel.rowCount();

            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const name = "recent-project-bundle";
            const expectedPath = tempDir + "/" + name + ".cw";

            saveAsDialogId.bundleFormat = true;
            saveAsDialogId.destinationFolder = tempDir;
            saveAsDialogId._pendingName = name;
            saveAsDialogId.bundleFilePath = expectedPath;
            saveAsDialogId.accepted();
            TestHelper.waitForProjectSaveToFinish(RootData.project);
            tryVerify(function() { return !RootData.project.isTemporaryProject; });

            tryVerify(function() {
                return RootData.recentProjectModel.rowCount() === before + 1;
            }, 5000, "recent project model should gain one entry after saveAs to bundle");
        }

        // Saving via the AskToSaveDialog → SaveAsDialog path must also update the recent projects model.
        // This is the actual quit flow: AskToSaveDialog "Save" → SaveAsDialog → fileSaved → addToRecents.
        function test_askToSaveDialog_tempProject_addsToRecentProjects() {
            RootData.region.addCave();
            tryVerify(function() { return !RootData.project.isNewEmptyProject(); });
            tryVerify(function() { return RootData.project.isModified(); });

            const before = RootData.recentProjectModel.rowCount();
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const name = "ask-save-recent";
            const expectedPath = tempDir + "/" + name + ".cw";

            askToSaveDialogId.askToSave();
            verify(askToSaveDialogId._dialog !== null);
            verify(askToSaveDialogId._dialog.askToSaveDialog.visible);

            // Accept AskToSaveDialog to open SaveAsDialog
            askToSaveDialogId._dialog.askToSaveDialog.accepted();
            saveAsDialogId.bundleFormat = true;
            saveAsDialogId.destinationFolder = tempDir;
            saveAsDialogId._pendingName = name;
            saveAsDialogId.bundleFilePath = expectedPath;
            saveAsDialogId.accepted();

            TestHelper.waitForProjectSaveToFinish(RootData.project);
            tryVerify(function() { return !RootData.project.isTemporaryProject; });

            tryVerify(function() {
                return RootData.recentProjectModel.rowCount() === before + 1;
            }, 5000, "recent project model should gain one entry after saving via AskToSaveDialog");
        }

        // Discard on a .cwproj directory project: discardChanges() reverts the git working dir
        function test_discard_revertsDirectoryProject() {
            TestHelper.loadProjectFromFile(RootData.project,
                                           "://datasets/test_cwProject/Phake Cave 3000.cw");
            tryVerify(function() { return RootData.region.caveCount > 0; });
            saveAsDirectory("askToSave-discard");

            // Rename a cave to make the project modified
            RootData.region.cave(0).name = "Cave After Discard";
            tryVerify(function() { return RootData.project.isModified(); });
            // Flush the rename save so the file exists on disk before we reset
            TestHelper.waitForProjectSaveToFinish(RootData.project);

            askToSaveDialogId.askToSave();
            verify(askToSaveDialogId._dialog !== null,
                   "dialog should be open for a modified project");

            // Trigger the Discard button
            askToSaveDialogId._dialog.askToSaveDialog.discarded();

            tryVerify(function() { return rootId.afterSaveCalled; }, 5000,
                      "afterSaveFunc should be called after discarding");
            tryVerify(function() {
                return TestHelper.projectModifiedFileCount(RootData.project) === 0;
            }, 5000, "git working directory should be clean after discardChanges()");
        }
    }
}
