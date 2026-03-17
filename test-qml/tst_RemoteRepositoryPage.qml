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
            for (let i = 0; i < RootData.remote.accountModel.rowCount(); ++i) {
                let accountIndex = RootData.remote.accountModel.index(i, 0)
                let provider = RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.ProviderRole)
                let accountUsername = RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.UsernameRole)
                let providerNumber = Number(provider)
                let accountUsernameNormalized = String(accountUsername).trim().toLowerCase()
                if (providerNumber === Number(RemoteAccountModel.GitHub)
                        && (expectedUsername.length === 0 || accountUsernameNormalized === expectedUsername)) {
                    return true
                }
            }
            return false
        }

        function githubAccountId(username) {
            let expectedUsername = String(username).trim().toLowerCase()
            for (let i = 0; i < RootData.remote.accountModel.rowCount(); ++i) {
                let accountIndex = RootData.remote.accountModel.index(i, 0)
                let provider = RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.ProviderRole)
                let accountUsername = RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.UsernameRole)
                if (Number(provider) === Number(RemoteAccountModel.GitHub)
                        && String(accountUsername).trim().toLowerCase() === expectedUsername) {
                    return String(RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.AccountIdRole))
                }
            }
            return ""
        }

        function githubAccountAuthState(accountId) {
            let expectedId = String(accountId).trim()
            for (let i = 0; i < RootData.remote.accountModel.rowCount(); ++i) {
                let accountIndex = RootData.remote.accountModel.index(i, 0)
                let candidateId = String(RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.AccountIdRole))
                if (candidateId !== expectedId) {
                    continue
                }
                return Number(RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.AuthStateRole))
            }
            return Number(RemoteAccountModel.Unknown)
        }

        function findObjectWithText(rootObject, expectedText) {
            if (rootObject === null || rootObject === undefined) {
                return null
            }

            if (rootObject.visible === true && rootObject.text !== undefined
                    && String(rootObject.text) === expectedText) {
                return rootObject
            }

            if (rootObject.children === undefined || rootObject.children === null) {
                return null
            }

            for (let i = 0; i < rootObject.children.length; ++i) {
                let result = findObjectWithText(rootObject.children[i], expectedText)
                if (result !== null) {
                    return result
                }
            }
            return null
        }

        function firstGitHubUsername() {
            for (let i = 0; i < RootData.remote.accountModel.rowCount(); ++i) {
                let accountIndex = RootData.remote.accountModel.index(i, 0)
                let provider = RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.ProviderRole)
                let providerNumber = Number(provider)
                if (providerNumber === Number(RemoteAccountModel.GitHub)) {
                    return String(RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.UsernameRole))
                }
            }
            return ""
        }

        function forgetAllGitHubAccountsViaCoordinator() {
            let guard = 0
            while (hasGitHubAccount() && guard < 20) {
                let username = firstGitHubUsername()
                if (username.length === 0) {
                    break
                }
                RootData.remote.accountCoordinator.forgetGitHubAccount(username)
                ++guard
            }
        }

        function addGithubAccountThroughDeviceFlow(previousUsername) {
            let remoteAccountCombo_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->remoteRepositoryPage->remoteAccountCombo")
            mouseClick(remoteAccountCombo_obj1)

            let addAccountEntry = null
            tryVerify(() => {
                addAccountEntry = ObjectFinder.findObjectByChain(mainWindow, "Popup->remoteAddAccountEntry")
                return addAccountEntry !== null
            }, 100000)
            mouseClick(addAccountEntry)

            let copyAndOpenButton = null
            tryVerify(() => {
                copyAndOpenButton = findChild(mainWindow, "remoteGitHubCopyOpenButton")
                return RootData.remote.gitHubIntegration.authState === GitHubIntegration.AwaitingVerification
                        && copyAndOpenButton !== null
                        && copyAndOpenButton.visible
            }, 30000)
            mouseClick(copyAndOpenButton)

            tryVerify(() => {
                let username = RootData.remote.gitHubIntegration.username
                let isAuthorized = RootData.remote.gitHubIntegration.authState === GitHubIntegration.Authorized
                let hasAccount = hasGitHubAccount(username)
                return isAuthorized
                        && username.length > 0
                        && hasAccount
            }, 300000)

            let connectedUsername = RootData.remote.gitHubIntegration.username
            let previousNormalized = previousUsername === undefined ? "" : String(previousUsername).trim().toLowerCase()
            if (previousNormalized.length > 0) {
                let switchedToDifferentUser = false
                tryVerify(() => {
                    let currentUsername = String(RootData.remote.gitHubIntegration.username).trim().toLowerCase()
                    switchedToDifferentUser = currentUsername.length > 0
                                            && currentUsername !== previousNormalized
                                            && hasGitHubAccount(currentUsername)
                    return switchedToDifferentUser
                }, 120000)
                verify(switchedToDifferentUser,
                       "Second device-flow authorization did not resolve to a different GitHub username. "
                       + "Please complete device auth using a different GitHub account (not the first account).")
                connectedUsername = RootData.remote.gitHubIntegration.username
            }

            verify(connectedUsername.length > 0)
            verify(hasGitHubAccount(connectedUsername))
            // console.log("[Assisted Debug] addGithubAccountThroughDeviceFlow username=" + connectedUsername
                        // + " accountId=" + githubAccountId(connectedUsername))
            return connectedUsername
        }

        function debugLogGitHubAccounts(contextLabel) {
            let parts = []
            for (let i = 0; i < RootData.remote.accountModel.rowCount(); ++i) {
                let accountIndex = RootData.remote.accountModel.index(i, 0)
                let provider = Number(RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.ProviderRole))
                if (provider !== Number(RemoteAccountModel.GitHub)) {
                    continue
                }
                let username = String(RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.UsernameRole))
                let accountId = String(RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.AccountIdRole))
                let active = Boolean(RootData.remote.accountModel.data(accountIndex, RemoteAccountModel.ActiveRole))
                parts.push("#" + i + " username=" + username + " accountId=" + accountId + " active=" + active)
            }
            console.log("[Assisted Debug] " + contextLabel
                        + " activeAccountId=" + RootData.remote.gitHubIntegration.activeAccountId
                        + " authState=" + RootData.remote.gitHubIntegration.authState
                        + " accounts=[" + parts.join(" | ") + "]")
        }

        function resolvedMainWindow() {
            if (mainWindow && mainWindow.window && mainWindow.window.contentItem !== undefined) {
                return mainWindow.window
            }
            if (mainWindow && mainWindow.Window && mainWindow.Window.window
                    && mainWindow.Window.window.contentItem !== undefined) {
                return mainWindow.Window.window
            }
            if (window && window.contentItem !== undefined) {
                return window
            }
            return null
        }

        function ensureMainWindowShown() {
            let testWindow = resolvedMainWindow()

            tryVerify(() => {
                testWindow = resolvedMainWindow()
                return testWindow !== null
                        && testWindow.visible
                        && testWindow.visibility !== Window.Hidden
            }, 10000)

            if (testWindow && typeof testWindow.raise === "function") {
                testWindow.raise()
            }
            if (testWindow && typeof testWindow.requestActivate === "function") {
                testWindow.requestActivate()
            }
        }

        function openAccountPopupAndClickEntry(accountId) {
            let remoteAccountCombo = ObjectFinder.findObjectByChain(mainWindow, "rootId->remoteRepositoryPage->remoteAccountCombo")
            verify(remoteAccountCombo !== null)

            let clicked = false
            tryVerify(() => {
                ensureMainWindowShown()
                if (remoteAccountCombo.window && remoteAccountCombo.window.visible !== undefined) {
                    if (!remoteAccountCombo.window.visible || remoteAccountCombo.window.visibility === Window.Hidden) {
                        return false
                    }
                }

                mouseClick(remoteAccountCombo)

                let entry = ObjectFinder.findObjectByChain(mainWindow, "Popup->remoteUserAccountEntry_" + accountId)
                if (entry === null || (entry.visible !== undefined && !entry.visible)) {
                    return false
                }

                mouseClick(entry)
                clicked = true
                return true
            }, 5000)
            verify(clicked)
        }

        function init() {
            RootData.recentProjectModel.clear()
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

            RootData.recentProjectModel.defaultRepositoryDir = TestHelper.toLocalUrl(fixture.cloneParentPath)

            let manualUrlField = findChild(mainWindow, "remoteManualUrlField")
            verify(manualUrlField !== null)
            manualUrlField.textField.text = fixture.remoteUrl

            let cloneButton = findChild(mainWindow, "remoteCloneButton")
            verify(cloneButton !== null)
            mouseClick(cloneButton)

            tryVerify(() => {
                return RootData.recentProjectModel.rowCount() === 1
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

            testerAssistedGate.beginDecision(
                        "Clone PhakeCave3000 using ssh.",
                        "Make sure ssh keys are uploaded");
            tryVerify(() => {
                return testerAssistedGate.decisionReady
            }, testerAssistedGate.decisionTimeoutMs)

            if (!testerAssistedGate.runCurrentTest) {
                skip("Tester-assisted tests were skipped by dialog choice or timeout.")
                return
            }

            RootData.recentProjectModel.clear()
            RootData.pageSelectionModel.currentPageAddress = "Remote"
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "remoteRepositoryPage"
            })

            let parentDir = TestHelper.tempDirectoryUrl()
            RootData.recentProjectModel.defaultRepositoryDir = parentDir

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
                return RootData.recentProjectModel.rowCount() === 1
            }, 120000)

            tryVerify(() => {
                return RootData.project.filename !== ""
                       && RootData.pageSelectionModel.currentPageAddress === "View"
            }, 120000)

            let originUrl = ""
            tryVerify(() => {
                let repoPath = RootData.recentProjectModel.data(RootData.recentProjectModel.index(0, 0), RecentProjectModel.PathRole)
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

            forgetAllGitHubAccountsViaCoordinator()
            tryVerify(() => {
                return RootData.remote.gitHubIntegration.authState === GitHubIntegration.Idle
                        && !hasGitHubAccount()
            }, 10000)


            let connectedUsername = addGithubAccountThroughDeviceFlow()

            let repositoryActionsButton = null
            tryVerify(() => {
                repositoryActionsButton = findChild(mainWindow, "remoteRepositoryActionsButton")
                return repositoryActionsButton !== null && repositoryActionsButton.visible && !RootData.remote.gitHubIntegration.busy
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
                return RootData.remote.gitHubIntegration.authState === GitHubIntegration.Idle
                        && remotePage.state === "none"
                        && !hasGitHubAccount(connectedUsername)
            }, 3000)
        }

        function test_testerAssisted_twoGithubAccounts_switchActiveAccount() {
            testerAssistedGate.beginDecision(
                        "Switch active GitHub account",
                        "The test will add one GitHub account through device flow.\n"
                        + "Then it adds a second GitHub account and switches between both automatically.")
            tryVerify(() => {
                return testerAssistedGate.decisionReady
            }, testerAssistedGate.decisionTimeoutMs)

            if (!testerAssistedGate.runCurrentTest) {
                skip("Tester-assisted tests were skipped by dialog choice or timeout.")
                return
            }

            RootData.pageSelectionModel.currentPageAddress = "Remote"
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "remoteRepositoryPage"
            })

            forgetAllGitHubAccountsViaCoordinator()
            tryVerify(() => {
                return RootData.remote.gitHubIntegration.authState === GitHubIntegration.Idle
                        && !hasGitHubAccount()
            }, 10000)

            let firstUsername = addGithubAccountThroughDeviceFlow()
            let firstAccountId = githubAccountId(firstUsername)
            verify(firstAccountId.length > 0)
            compare(RootData.remote.gitHubIntegration.activeAccountId, firstAccountId)

            let secondUsername = addGithubAccountThroughDeviceFlow(firstUsername)
            let firstNormalized = String(firstUsername).trim().toLowerCase()
            let secondNormalized = String(secondUsername).trim().toLowerCase()

            let secondAccountId = githubAccountId(secondUsername)
            verify(secondAccountId.length > 0)
            compare(RootData.remote.gitHubIntegration.activeAccountId, secondAccountId)

            openAccountPopupAndClickEntry(secondAccountId)

            tryVerify(() => {
                return RootData.remote.gitHubIntegration.activeAccountId === secondAccountId
                        && RootData.remote.accountModel.activeAccountId(RemoteAccountModel.GitHub) === secondAccountId
            }, 3000)

            openAccountPopupAndClickEntry(firstAccountId)

            tryVerify(() => {
                return RootData.remote.gitHubIntegration.activeAccountId === firstAccountId
                        && RootData.remote.accountModel.activeAccountId(RemoteAccountModel.GitHub) === firstAccountId
            }, 3000)

            forgetAllGitHubAccountsViaCoordinator()
        }

        function test_testerAssisted_invalidGithubToken_revokesAccount() {
            testerAssistedGate.beginDecision(
                        "Invalid GitHub token revokes account",
                        "The test will add one GitHub account through device flow.\n"
                        + "Then it corrupts the stored token in keychain and verifies revoke/error handling.\n"
                        + "After the revoke message appears, please reconnect the same account in the next device-flow step.")
            tryVerify(() => {
                return testerAssistedGate.decisionReady
            }, testerAssistedGate.decisionTimeoutMs)

            if (!testerAssistedGate.runCurrentTest) {
                skip("Tester-assisted tests were skipped by dialog choice or timeout.")
                return
            }

            RootData.pageSelectionModel.currentPageAddress = "Remote"
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "remoteRepositoryPage"
            })

            forgetAllGitHubAccountsViaCoordinator()
            tryVerify(() => {
                return RootData.remote.gitHubIntegration.authState === GitHubIntegration.Idle
                        && !hasGitHubAccount()
            }, 10000)

            let username = addGithubAccountThroughDeviceFlow()
            let accountId = githubAccountId(username)
            verify(accountId.length > 0)

            verify(TestHelper.setGitHubAccessTokenForAccount(accountId, "cw-invalid-token-for-assisted-test"),
                   "Failed to overwrite active GitHub token in keychain")

            RootData.remote.gitHubIntegration.reloadAccessTokenFromCredentialStore()
            tryVerify(() => {
                return RootData.remote.gitHubIntegration.authState === GitHubIntegration.Authorized
            }, 10000)

            RootData.remote.gitHubIntegration.refreshRepositories()

            tryVerify(() => {
                let errorText = String(RootData.remote.gitHubIntegration.errorMessage).toLowerCase()
                return RootData.remote.gitHubIntegration.authState === GitHubIntegration.Error
                        && githubAccountAuthState(accountId) === Number(RemoteAccountModel.Revoked)
                        && errorText.indexOf("expired") !== -1
            }, 30000)

            let reconnectButton = findChild(mainWindow, "remoteGitHubConnectButton")
            verify(reconnectButton !== null)
            compare(reconnectButton.text, "Reconnect to GitHub")

            forgetAllGitHubAccountsViaCoordinator()
        }
    }
}
