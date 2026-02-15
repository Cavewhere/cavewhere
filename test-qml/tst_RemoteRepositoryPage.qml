import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TesterAssistedGateDialog {
        id: testerAssistedGate
        dialogParent: rootId
        autoSkipCountdownSeconds: 5
    }

    TestCase {
        name: "RemoteRepositoryPage"
        when: windowShown

        function hasGitHubAccount(username) {
            let expectedUsername = username === undefined ? "" : username.trim().toLowerCase()
            for (let i = 0; i < RootData.remoteAccountModel.rowCount(); ++i) {
                let accountIndex = RootData.remoteAccountModel.index(i, 0)
                let provider = RootData.remoteAccountModel.data(accountIndex, RemoteAccountModel.ProviderRole)
                let accountUsername = RootData.remoteAccountModel.data(accountIndex, RemoteAccountModel.UsernameRole)
                let providerNumber = Number(provider)
                let accountUsernameNormalized = String(accountUsername).trim().toLowerCase()
                if (providerNumber === Number(RemoteAccountModel.GitHub)
                        && (expectedUsername.length === 0 || accountUsernameNormalized === expectedUsername)) {
                    return true
                }
            }
            return false
        }

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

        function test_testerAssisted_addAndForgetGithubAccount() {
            testerAssistedGate.beginDecision(
                        "Add and forget GitHub account",
                        "The test will click through the UI automatically.\n"
                        + "You only need to complete GitHub device authorization when prompted.")
            tryVerify(() => {
                return testerAssistedGate.decisionReady
            }, testerAssistedGate.decisionTimeoutMs)

            if (!testerAssistedGate.runCurrentTest) {
                skip("Tester-assisted tests were skipped by dialog choice or timeout.")
                return
            }

            let remotePage = RootData.pageView.currentPageItem
            verify(remotePage !== null)
            compare(remotePage.objectName, "remoteRepositoryPage")

            RootData.gitHubIntegration.logout()
            RootData.remoteAccountModel.clear()
            tryVerify(() => {
                return RootData.gitHubIntegration.authState === GitHubIntegration.Idle
                        && !hasGitHubAccount()
            }, 10000)


            let remoteAccountCombo_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->remoteRepositoryPage->remoteAccountCombo")
            mouseClick(remoteAccountCombo_obj1)

            let addAccountEntry = null
            tryVerify(() => {
                addAccountEntry = ObjectFinder.findObjectByChain(mainWindow, "Popup->remoteAddAccountEntry")
                // addAccountEntry = findChild(mainWindow, "remoteAddAccountEntry")
                return addAccountEntry !== null
            }, 100000)
            mouseClick(addAccountEntry)
            // mouseClick(addAccountEntry)

            let copyAndOpenButton = null
            tryVerify(() => {
                copyAndOpenButton = findChild(mainWindow, "remoteGitHubCopyOpenButton")
                return RootData.gitHubIntegration.authState === GitHubIntegration.AwaitingVerification
                        && copyAndOpenButton !== null
                        && copyAndOpenButton.visible
            }, 30000)
            mouseClick(copyAndOpenButton)

            console.log("[Tester Assisted] Complete GitHub device authorization in the opened browser page.")
            tryVerify(() => {
                                  let username = RootData.gitHubIntegration.username

                                  let isAuthorized = RootData.gitHubIntegration.authState === GitHubIntegration.Authorized
                                  let hasAccount = hasGitHubAccount(username)

                                  return isAuthorized
                                  && username.length > 0
                                  && hasAccount
                      }, 300000)

            let connectedUsername = RootData.gitHubIntegration.username
            verify(connectedUsername.length > 0)
            verify(hasGitHubAccount(connectedUsername))

            let repositoryActionsButton = null
            tryVerify(() => {
                repositoryActionsButton = findChild(mainWindow, "remoteRepositoryActionsButton")
                return repositoryActionsButton !== null && repositoryActionsButton.visible && !RootData.gitHubIntegration.busy
            }, 3000)
            mouseClick(repositoryActionsButton)

            let forgetAccountMenuItem = null
            tryVerify(() => {
                forgetAccountMenuItem = findChild(mainWindow, "remoteForgetAccountMenuItem")
                return forgetAccountMenuItem !== null && forgetAccountMenuItem.visible && forgetAccountMenuItem.enabled
            }, 1000)
            mouseClick(forgetAccountMenuItem)

            let removeButton = null
            tryVerify(() => {
                removeButton = findChild(mainWindow, "remoteForgetAccountConfirmButton")
                return removeButton !== null && removeButton.visible
            }, 1000)
            mouseClick(removeButton)

            tryVerify(() => {
                return RootData.gitHubIntegration.authState === GitHubIntegration.Idle
                        && remotePage.state === "none"
                        && !hasGitHubAccount(connectedUsername)
            }, 3000)
        }
    }
}
