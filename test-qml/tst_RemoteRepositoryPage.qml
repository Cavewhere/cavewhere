import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "RemoteRepositoryPage"
        when: windowShown

        function init() {
            RootData.repositoryModel.clear()
            RootData.pageSelectionModel.currentPageAddress = "Remote"
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "remoteRepositoryPage"
            })
        }

        function test_cloneLocalBareRemote() {
            let fixture = TestHelper.createLocalBareRemoteForCloneTest()
            compare(fixture.errorMessage, "")
            verify(fixture.remoteUrl !== "")
            verify(fixture.cloneParentPath !== "")
            verify(fixture.expectedClonePath !== "")

            RootData.repositoryModel.defaultRepositoryDir = TestHelper.toLocalUrl(fixture.cloneParentPath)

            let manualUrlField = findChild(mainWindow, "remoteManualUrlField")
            verify(manualUrlField !== null)
            manualUrlField.textField.text = fixture.remoteUrl

            let cloneButton = findChild(mainWindow, "remoteCloneButton")
            verify(cloneButton !== null)
            mouseClick(cloneButton)

            tryVerify(() => {
                return RootData.repositoryModel.rowCount() === 1
            }, 20000)

            compare(TestHelper.directoryExists(TestHelper.toLocalUrl(fixture.expectedClonePath)), true)

            let cloneErrorArea = findChild(mainWindow, "remoteCloneErrorArea")
            verify(cloneErrorArea !== null)
            compare(cloneErrorArea.visible, false)

            let cloneStatusText = findChild(mainWindow, "remoteCloneStatusText")
            verify(cloneStatusText !== null)
            tryVerify(() => {
                return cloneStatusText.text.indexOf("Clone complete.") >= 0
            }, 5000)

            tryVerify(() => {
                return RootData.project.filename !== ""
                       && RootData.project.filename.indexOf(fixture.expectedClonePath) >= 0
            }, 10000)
            compare(RootData.pageSelectionModel.currentPageAddress, "View")

            let configuredRemoteUrl = TestHelper.repositoryRemoteUrl(TestHelper.toLocalUrl(fixture.expectedClonePath), "origin")
            verify(configuredRemoteUrl !== "")
            compare(TestHelper.normalizeFileGitUrl(configuredRemoteUrl),
                    TestHelper.normalizeFileGitUrl(fixture.remoteUrl))
        }

        function test_repro_cloneGithubPhakeCave3000() {
            if (TestHelper.environmentVariable("CW_QML_REPRO_GITHUB_CLONE") !== "1") {
                skip("Set CW_QML_REPRO_GITHUB_CLONE=1 to run SSH GitHub clone repro test.")
                return
            }

            RootData.repositoryModel.clear()
            RootData.pageSelectionModel.currentPageAddress = "Remote"
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "remoteRepositoryPage"
            })

            let parentDir = TestHelper.tempDirectoryUrl()
            RootData.repositoryModel.defaultRepositoryDir = parentDir

            let manualUrlField = findChild(mainWindow, "remoteManualUrlField")
            verify(manualUrlField !== null)
            manualUrlField.textField.text = "git@github.com:Cavewhere/PhakeCave3000.git"

            let cloneButton = findChild(mainWindow, "remoteCloneButton")
            verify(cloneButton !== null)
            mouseClick(cloneButton)

            // Repro target:
            // - clone succeeds
            // - page auto-opens repo
            // - current bug path may assert during load/rehydration.
            tryVerify(() => {
                return RootData.repositoryModel.rowCount() === 1
            }, 120000)

            tryVerify(() => {
                return RootData.project.filename !== ""
                       && RootData.pageSelectionModel.currentPageAddress === "View"
            }, 120000)

            let originUrl = ""
            tryVerify(() => {
                let repoPath = RootData.repositoryModel.data(RootData.repositoryModel.index(0, 0), RepositoryModel.PathRole)
                originUrl = TestHelper.repositoryRemoteUrl(TestHelper.toLocalUrl(repoPath), "origin")
                return originUrl !== ""
            }, 30000)
            compare(TestHelper.normalizeFileGitUrl(originUrl),
                    TestHelper.normalizeFileGitUrl("ssh://git@github.com/Cavewhere/PhakeCave3000.git"))

            tryVerify(() => {
                return RootData.region.caveCount > 0
            }, 120000)

            let foundPhakeCave = false
            for (let i = 0; i < RootData.region.caveCount; ++i) {
                let cave = RootData.region.cave(i)
                if (cave !== null && cave.name === "Phake Cave 3000") {
                    foundPhakeCave = true
                    break
                }
            }
            verify(foundPhakeCave)
        }
    }
}
