import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

Item {
    id: rootId
    width: 400
    height: 300

    SaveAsDialog {
        id: saveAsDialogId
    }

    CWTestCase {
        name: "SaveAsDialog"
        when: windowShown

        function init() {
            RootData.newProject();
            RootData.recentProjectModel.clear();
            tryVerify(function() { return RootData.project.isTemporaryProject; });
        }

        // ── Fixtures ──────────────────────────────────────────────────────────

        function loadLegacyFixture() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");
            tryVerify(function() {
                return RootData.region.caveCount > 0;
            });
        }

        function projectSnapshot() {
            let caveCount = RootData.region.caveCount;
            let tripCount = 0;
            let firstTripStations = 0;
            let firstCaveName = "";
            let firstTripName = "";

            if (caveCount > 0) {
                const cave = RootData.region.cave(0);
                firstCaveName = cave.name;
                tripCount = cave.tripCount;

                if (tripCount > 0) {
                    const trip = cave.trip(0);
                    firstTripName = trip.name;
                    firstTripStations = trip.numberOfStations();
                }
            }

            return {
                caveCount: caveCount,
                tripCount: tripCount,
                firstTripStations: firstTripStations,
                firstCaveName: firstCaveName,
                firstTripName: firstTripName
            };
        }

        function compareSnapshots(expected, actual) {
            compare(actual.caveCount, expected.caveCount);
            compare(actual.tripCount, expected.tripCount);
            compare(actual.firstTripStations, expected.firstTripStations);
            compare(actual.firstCaveName, expected.firstCaveName);
            compare(actual.firstTripName, expected.firstTripName);
        }

        // ── Test helper ───────────────────────────────────────────────────────
        //
        // Drives a save programmatically without opening the native file pickers.
        // localPath may include or omit an extension; this helper strips it and
        // derives the canonical save path the same way the new dialog does.
        //
        // Returns the expected on-disk path so callers can verify it exists.
        function saveAsViaDialog(localPath, isBundleFormat) {
            // Strip any existing extension so we work with the bare name.
            const lower = localPath.toLowerCase();
            let cleanPath = localPath;
            if (lower.endsWith(".cwproj")) { cleanPath = localPath.slice(0, -7); }
            else if (lower.endsWith(".cw")) { cleanPath = localPath.slice(0, -3); }

            const slashIdx = Math.max(cleanPath.lastIndexOf("/"), cleanPath.lastIndexOf("\\"));
            const name = slashIdx >= 0 ? cleanPath.slice(slashIdx + 1) : cleanPath;
            const parentDir = slashIdx >= 0 ? cleanPath.slice(0, slashIdx) : "";

            saveAsDialogId.bundleFormat = isBundleFormat;
            saveAsDialogId.destinationFolder = parentDir;
            saveAsDialogId._pendingName = name;
            saveAsDialogId.bundleFilePath = isBundleFormat ? (parentDir + "/" + name + ".cw") : "";

            saveAsDialogId.accepted();

            const expectedPath = isBundleFormat
                    ? parentDir + "/" + name + ".cw"
                    : parentDir + "/" + name + "/" + name + ".cwproj";

            TestHelper.waitForProjectSaveToFinish(RootData.project);
            tryVerify(function() {
                return TestHelper.fileExists(TestHelper.toLocalUrl(expectedPath));
            });

            return expectedPath;
        }

        // ── Format detection via fileType ─────────────────────────────────────

        // Loading a legacy SQLite .cw file converts to GitFileType (directory).
        function test_bundledFirst_forLegacyProject() {
            loadLegacyFixture();
            compare(RootData.project.fileType, Project.GitFileType);
        }

        function test_bundledFirst_defaultsToDirectory_forDirectoryProject() {
            loadLegacyFixture();
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            saveAsViaDialog(tempDir + "/bundledFirst-dir", false);

            RootData.project.loadFile(TestHelper.toLocalUrl(
                        tempDir + "/bundledFirst-dir/bundledFirst-dir.cwproj"));
            tryVerify(function() { return RootData.region.caveCount > 0; });

            compare(RootData.project.fileType, Project.GitFileType);
        }

        // After explicitly saving as a .cw bundle, fileType becomes BundledGitFileType.
        function test_bundledFirst_forBundledProject() {
            loadLegacyFixture();
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            saveAsViaDialog(tempDir + "/bundled-project", true);

            compare(RootData.project.fileType, Project.BundledGitFileType);
        }

        // ── resolvedSavePath computation ──────────────────────────────────────

        function test_resolvedSavePath_forDirectory() {
            saveAsDialogId.bundleFormat = false;
            saveAsDialogId.destinationFolder = "/tmp/parent";
            saveAsDialogId._pendingName = "MyProject";
            saveAsDialogId.bundleFilePath = "";
            compare(saveAsDialogId.resolvedSavePath,
                    "/tmp/parent/MyProject/MyProject.cwproj");
        }

        function test_resolvedSavePath_forBundle_derived() {
            saveAsDialogId.bundleFormat = true;
            saveAsDialogId.destinationFolder = "/tmp/parent";
            saveAsDialogId._pendingName = "MyProject";
            saveAsDialogId.bundleFilePath = "";
            compare(saveAsDialogId.resolvedSavePath,
                    "/tmp/parent/MyProject.cw");
        }

        function test_resolvedSavePath_forBundle_explicit() {
            saveAsDialogId.bundleFormat = true;
            saveAsDialogId.bundleFilePath = "/tmp/parent/CustomName.cw";
            compare(saveAsDialogId.resolvedSavePath,
                    "/tmp/parent/CustomName.cw");
        }

        function test_resolvedSavePath_sanitizesUnsafeChars() {
            saveAsDialogId.bundleFormat = false;
            saveAsDialogId.destinationFolder = "/tmp/parent";
            saveAsDialogId._pendingName = "My<Bad>Name";
            saveAsDialogId.bundleFilePath = "";
            compare(saveAsDialogId.resolvedSavePath,
                    "/tmp/parent/My_Bad_Name/My_Bad_Name.cwproj");
        }

        // ── isTemporary reflects project state ────────────────────────────────

        function test_isTemporary_trueForNewProject() {
            verify(RootData.project.isTemporaryProject);
        }

        function test_isTemporary_falseAfterFirstSave() {
            loadLegacyFixture();
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            saveAsViaDialog(tempDir + "/temp-check-project", false);
            verify(!RootData.project.isTemporaryProject);
        }

        // ── Regression: typing must not mutate RootData.region.name ──────────

        // Verifies that editing _pendingName (simulating user typing in the
        // project name field) does NOT write through to RootData.region.name.
        // The resolved path must update while the region name stays unchanged.
        function test_typingDoesNotMutateRegionName() {
            loadLegacyFixture();
            const originalName = RootData.region.name;
            verify(originalName.length > 0);

            saveAsDialogId._pendingName = "typing-new-name";

            // Region name must be untouched.
            compare(RootData.region.name, originalName);

            // Path preview must reflect the new pending name.
            saveAsDialogId.bundleFormat = false;
            saveAsDialogId.destinationFolder = "/tmp/parent";
            saveAsDialogId.bundleFilePath = "";
            verify(saveAsDialogId.resolvedSavePath.indexOf("typing-new-name") >= 0);
        }

        // ── Save and reload round-trips ───────────────────────────────────────

        function test_saveAsDirectory_fromLegacy() {
            loadLegacyFixture();
            const before = projectSnapshot();
            verify(before.caveCount > 0);

            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const savedPath = saveAsViaDialog(tempDir + "/qml-saveas-directory.cwproj", false);

            compare(RootData.project.fileType, Project.GitFileType);
            RootData.project.loadFile(TestHelper.toLocalUrl(savedPath));
            tryVerify(function() {
                return RootData.region.caveCount === before.caveCount;
            });
            compareSnapshots(before, projectSnapshot());
        }

        function test_saveAsBundle_fromLegacy() {
            loadLegacyFixture();
            const before = projectSnapshot();
            verify(before.caveCount > 0);

            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const savedPath = saveAsViaDialog(tempDir + "/qml-saveas-bundle.cw", true);

            compare(RootData.project.fileType, Project.BundledGitFileType);
            RootData.project.loadFile(TestHelper.toLocalUrl(savedPath));
            tryVerify(function() {
                return RootData.region.caveCount === before.caveCount;
            });
            compareSnapshots(before, projectSnapshot());
        }

        function test_saveAsBundle_addsBundlePathToRecentProjectModel() {
            loadLegacyFixture();
            RootData.recentProjectModel.clear();
            compare(RootData.recentProjectModel.rowCount(), 0);

            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const savedPath = saveAsViaDialog(tempDir + "/qml-saveas-recent-bundle.cw", true);

            tryVerify(function() {
                return RootData.recentProjectModel.rowCount() === 1;
            });

            const storedPath = RootData.recentProjectModel.data(
                        RootData.recentProjectModel.index(0, 0),
                        RecentProjectModel.PathRole);
            compare(storedPath, savedPath);
        }

        function test_saveAsDirectory_withoutExtension_fromLegacy() {
            loadLegacyFixture();
            const before = projectSnapshot();
            verify(before.caveCount > 0);

            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const savedPath = saveAsViaDialog(tempDir + "/qml-saveas-directory-noext", false);

            compare(RootData.project.fileType, Project.GitFileType);
            RootData.project.loadFile(TestHelper.toLocalUrl(savedPath));
            tryVerify(function() {
                return RootData.region.caveCount === before.caveCount;
            });
            compareSnapshots(before, projectSnapshot());
        }

        function test_saveAsBundle_withoutExtension_fromLegacy() {
            loadLegacyFixture();
            const before = projectSnapshot();
            verify(before.caveCount > 0);

            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const savedPath = saveAsViaDialog(tempDir + "/qml-saveas-bundle-noext", true);

            compare(RootData.project.fileType, Project.BundledGitFileType);
            RootData.project.loadFile(TestHelper.toLocalUrl(savedPath));
            tryVerify(function() {
                return RootData.region.caveCount === before.caveCount;
            });
            compareSnapshots(before, projectSnapshot());
        }

        // ── Conflict detection ────────────────────────────────────────────────

        function test_directoryConflict_blocksWhenFolderExists() {
            loadLegacyFixture();
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            saveAsViaDialog(tempDir + "/conflict-dir", false);

            // _directoryConflict was cached as false during saveAsViaDialog (directory
            // didn't exist when the binding first evaluated). Force re-evaluation now
            // that the save is complete by toggling destinationFolder.
            saveAsDialogId.destinationFolder = "";
            saveAsDialogId.bundleFormat = false;
            saveAsDialogId._pendingName = "conflict-dir";
            saveAsDialogId.bundleFilePath = "";
            saveAsDialogId.destinationFolder = tempDir;

            verify(saveAsDialogId._directoryConflict);
            verify(!saveAsDialogId._bundleConflict);
        }

        function test_directoryConflict_clearsWhenNameChanges() {
            loadLegacyFixture();
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            saveAsViaDialog(tempDir + "/conflict-dir2", false);

            // Force re-evaluation after the directory has been created.
            saveAsDialogId.destinationFolder = "";
            saveAsDialogId.bundleFormat = false;
            saveAsDialogId._pendingName = "conflict-dir2";
            saveAsDialogId.bundleFilePath = "";
            saveAsDialogId.destinationFolder = tempDir;
            verify(saveAsDialogId._directoryConflict);

            // Changing the name to something that doesn't exist clears the conflict.
            saveAsDialogId._pendingName = "conflict-dir2-new";
            verify(!saveAsDialogId._directoryConflict);
        }

        function test_bundleConflict_warnsWhenFileExists() {
            loadLegacyFixture();
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            saveAsViaDialog(tempDir + "/conflict-bundle.cw", true);

            // _bundleConflict was cached false during saveAsViaDialog. Force re-evaluation
            // by toggling bundleFormat after the file exists on disk.
            saveAsDialogId.bundleFilePath = "";
            saveAsDialogId.bundleFormat = false;
            saveAsDialogId.destinationFolder = tempDir;
            saveAsDialogId._pendingName = "conflict-bundle";
            saveAsDialogId.bundleFormat = true;

            verify(!saveAsDialogId._directoryConflict);
            verify(saveAsDialogId._bundleConflict);
        }

        function test_bundleConflict_falseInDirectoryMode() {
            loadLegacyFixture();
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            saveAsViaDialog(tempDir + "/no-conflict-bundle.cw", true);

            // Switch to directory mode — _bundleConflict must be false.
            saveAsDialogId.bundleFilePath = "";
            saveAsDialogId.bundleFormat = false;
            saveAsDialogId.destinationFolder = tempDir;
            saveAsDialogId._pendingName = "no-conflict-bundle";

            verify(!saveAsDialogId._bundleConflict);
        }

        // ── _parentNotFound validation ────────────────────────────────────────

        function test_parentNotFound_trueWhenFolderMissing() {
            saveAsDialogId.bundleFormat = false;
            saveAsDialogId.destinationFolder = "/nonexistent/path/that/does/not/exist";
            saveAsDialogId._pendingName = "MyProject";
            saveAsDialogId.bundleFilePath = "";
            verify(saveAsDialogId._parentNotFound);
        }

        function test_parentNotFound_falseWhenFolderExists() {
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            saveAsDialogId.bundleFormat = false;
            saveAsDialogId.destinationFolder = tempDir;
            saveAsDialogId._pendingName = "MyProject";
            saveAsDialogId.bundleFilePath = "";
            verify(!saveAsDialogId._parentNotFound);
        }

        function test_parentNotFound_falseInBundleMode() {
            // Bundle mode does not require the parent to exist.
            saveAsDialogId.bundleFormat = true;
            saveAsDialogId.destinationFolder = "/nonexistent/path/that/does/not/exist";
            saveAsDialogId._pendingName = "MyProject";
            saveAsDialogId.bundleFilePath = "";
            verify(!saveAsDialogId._parentNotFound);
        }

        function test_parentNotFound_clearsWhenFolderCreated() {
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            // Start with a non-existent subfolder.
            saveAsDialogId.bundleFormat = false;
            saveAsDialogId.destinationFolder = tempDir + "/does-not-exist";
            saveAsDialogId._pendingName = "MyProject";
            saveAsDialogId.bundleFilePath = "";
            verify(saveAsDialogId._parentNotFound);

            // Switch to an existing folder — error must clear.
            saveAsDialogId.destinationFolder = tempDir;
            verify(!saveAsDialogId._parentNotFound);
        }

        // ── _normalizeBundleFilePath (macOS double-extension regression) ──────
        //
        // macOS SaveFile dialogs can return paths with stale or doubled extensions
        // (e.g. "test.cwproj.cw" when the user typed "test" with a .cwproj name
        // already in the filename field). _normalizeBundleFilePath strips all
        // trailing .cw/.cwproj extensions and then appends exactly one .cw.

        function test_normalizeBundleFilePath_noExtension() {
            compare(saveAsDialogId._normalizeBundleFilePath("/tmp/test"),
                    "/tmp/test.cw");
        }

        function test_normalizeBundleFilePath_withCwExtension() {
            compare(saveAsDialogId._normalizeBundleFilePath("/tmp/test.cw"),
                    "/tmp/test.cw");
        }

        function test_normalizeBundleFilePath_withCwprojExtension() {
            compare(saveAsDialogId._normalizeBundleFilePath("/tmp/test.cwproj"),
                    "/tmp/test.cw");
        }

        function test_normalizeBundleFilePath_doubleExtension_cwprojDotCw() {
            // Regression: macOS appended .cw on top of an existing .cwproj name.
            compare(saveAsDialogId._normalizeBundleFilePath("/tmp/test.cwproj.cw"),
                    "/tmp/test.cw");
        }

        // ── finalProjectPathForSelection backward-compat ──────────────────────

        function test_finalProjectPathForSelection_directory() {
            const result = saveAsDialogId.finalProjectPathForSelection(
                        "/tmp/foo", ".cwproj");
            compare(result, "/tmp/foo/foo.cwproj");
        }

        function test_finalProjectPathForSelection_bundle() {
            const result = saveAsDialogId.finalProjectPathForSelection(
                        "/tmp/foo", ".cw");
            compare(result, "/tmp/foo.cw");
        }

        function test_finalProjectPathForSelection_stripsExistingExtension_cwproj() {
            const result = saveAsDialogId.finalProjectPathForSelection(
                        "/tmp/foo.cwproj", ".cwproj");
            compare(result, "/tmp/foo/foo.cwproj");
        }

        function test_finalProjectPathForSelection_stripsExistingExtension_cw() {
            const result = saveAsDialogId.finalProjectPathForSelection(
                        "/tmp/foo.cw", ".cw");
            compare(result, "/tmp/foo.cw");
        }
    }
}
