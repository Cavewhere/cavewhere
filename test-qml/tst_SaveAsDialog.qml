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
            RootData.recentProjectModel.clear();
        }

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

        function saveAsViaAccepted(selectedPath, expectedPath) {
            saveAsDialogId.selectedFile = TestHelper.toLocalUrl(selectedPath);
            saveAsDialogId.selectedNameFilter.index = expectedPath.toLowerCase().endsWith(".cwproj") ? 1 : 0;
            saveAsDialogId.accepted();

            TestHelper.waitForProjectSaveToFinish(RootData.project);
            tryVerify(function() {
                return TestHelper.fileExists(TestHelper.toLocalUrl(expectedPath));
            });
        }

        function test_nameFilters_defaultToBundled_forLegacyProject() {
            loadLegacyFixture();

            compare(RootData.project.fileType, Project.BundledGitFileType);
            compare(saveAsDialogId.nameFilters[0].toString(), "Bundled (*.cw)");
            compare(saveAsDialogId.nameFilters[1].toString(), "Directory (*.cwproj)");
        }

        function test_nameFilters_defaultToDirectory_forDirectoryProject() {
            loadLegacyFixture();
            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const selectedPath = tempDir + "/namefilter-directory.cwproj";
            const expectedPath = tempDir + "/namefilter-directory/namefilter-directory.cwproj";
            saveAsViaAccepted(selectedPath, expectedPath);

            tryVerify(function() {
                return RootData.region.caveCount > 0;
            });

            RootData.project.loadFile(TestHelper.toLocalUrl(expectedPath));
            tryVerify(function() {
                return RootData.region.caveCount > 0;
            });

            compare(RootData.project.fileType, Project.GitFileType);
            compare(saveAsDialogId.nameFilters[0].toString(), "Directory (*.cwproj)");
            compare(saveAsDialogId.nameFilters[1].toString(), "Bundled (*.cw)");
        }

        function test_nameFilters_defaultToBundled_forBundledProject() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");
            tryVerify(function() {
                return RootData.region.caveCount > 0;
            });
            RootData.project.save();
            TestHelper.waitForProjectSaveToFinish(RootData.project);

            compare(RootData.project.fileType, Project.BundledGitFileType);
            compare(saveAsDialogId.nameFilters[0].toString(), "Bundled (*.cw)");
            compare(saveAsDialogId.nameFilters[1].toString(), "Directory (*.cwproj)");
        }

        function test_saveAsDirectory_fromLegacy() {
            loadLegacyFixture();

            const before = projectSnapshot();
            verify(before.caveCount > 0);

            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const selectedPath = tempDir + "/qml-saveas-directory.cwproj";
            const expectedPath = tempDir + "/qml-saveas-directory/qml-saveas-directory.cwproj";
            saveAsViaAccepted(selectedPath, expectedPath);

            compare(RootData.project.fileType, Project.GitFileType);
            RootData.project.loadFile(TestHelper.toLocalUrl(expectedPath));
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
            const expectedPath = tempDir + "/qml-saveas-bundle.cw";
            saveAsViaAccepted(expectedPath, expectedPath);

            compare(RootData.project.fileType, Project.BundledGitFileType);
            RootData.project.loadFile(TestHelper.toLocalUrl(expectedPath));
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
            const expectedPath = tempDir + "/qml-saveas-recent-bundle.cw";
            saveAsViaAccepted(expectedPath, expectedPath);

            tryVerify(function() {
                return RootData.recentProjectModel.rowCount() === 1;
            });

            const storedPath = RootData.recentProjectModel.data(
                        RootData.recentProjectModel.index(0, 0),
                        RecentProjectModel.PathRole);
            compare(storedPath, expectedPath);
        }

        function test_saveAsDirectory_withoutExtension_fromLegacy() {
            loadLegacyFixture();

            const before = projectSnapshot();
            verify(before.caveCount > 0);

            const tempDir = RootData.urlToLocal(TestHelper.tempDirectoryUrl());
            const selectedPathWithoutExtension = tempDir + "/qml-saveas-directory-noext";
            const expectedPath = tempDir + "/qml-saveas-directory-noext/qml-saveas-directory-noext.cwproj";
            saveAsViaAccepted(selectedPathWithoutExtension, expectedPath);

            compare(RootData.project.fileType, Project.GitFileType);
            RootData.project.loadFile(TestHelper.toLocalUrl(expectedPath));
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
            const selectedPathWithoutExtension = tempDir + "/qml-saveas-bundle-noext";
            const expectedPath = selectedPathWithoutExtension + ".cw";
            saveAsViaAccepted(selectedPathWithoutExtension, expectedPath);

            compare(RootData.project.fileType, Project.BundledGitFileType);
            RootData.project.loadFile(TestHelper.toLocalUrl(expectedPath));
            tryVerify(function() {
                return RootData.region.caveCount === before.caveCount;
            });

            compareSnapshots(before, projectSnapshot());
        }
    }
}
