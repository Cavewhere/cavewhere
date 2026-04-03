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

        function test_clone_asksToSave_whenProjectModified() {
            // Save a project and make it modified so ask-to-save is triggered.
            const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tmpPath + "/remote-ask-save.cwproj"))
            TestHelper.waitForProjectSaveToFinish(RootData.project)
            RootData.region.addCave()
            tryVerify(() => RootData.project.modified, 3000,
                      "project should become modified after adding a cave")

            let fixture = TestHelper.createLocalBareRemoteForCloneTest()
            compare(fixture.errorMessage, "")
            RootData.recentProjectModel.defaultRepositoryDir = TestHelper.toLocalUrl(fixture.cloneParentPath)

            let manualUrlField = findChild(mainWindow, "remoteManualUrlField")
            verify(manualUrlField !== null)
            manualUrlField.textField.text = fixture.remoteUrl

            let cloneButton = findChild(mainWindow, "remoteCloneButton")
            verify(cloneButton !== null)
            mouseClick(cloneButton)

            // Clone completes → AskToSaveDialog appears before loading.
            // Access via the injected dialog instance in MainWindowTest.
            let askToSaveDialog = findChild(rootId, "mainWindowTestAskToSaveDialog")
            verify(askToSaveDialog !== null, "mainWindowTestAskToSaveDialog not found")
            tryVerify(() => {
                return askToSaveDialog._dialog !== null
                       && askToSaveDialog._dialog.askToSaveDialog.visible
            }, 20000, "AskToSaveDialog should appear after clone completes")

            askToSaveDialog._dialog.askToSaveDialog.discarded()

            // Cloned project should open in View.
            tryVerify(() => {
                return RootData.project.filename.indexOf(fixture.expectedClonePath) >= 0
            }, 10000, "cloned project should be loaded")
            compare(RootData.pageSelectionModel.currentPageAddress, "View")
        }

        function test_clone_retryAfterFailure() {
            // Use a bad URL first — should fail fast with a connection/protocol error.
            let fixture = TestHelper.createLocalBareRemoteForCloneTest()
            compare(fixture.errorMessage, "")

            RootData.recentProjectModel.defaultRepositoryDir = TestHelper.toLocalUrl(fixture.cloneParentPath)

            let manualUrlField = findChild(mainWindow, "remoteManualUrlField")
            verify(manualUrlField !== null)
            manualUrlField.textField.text = "https://localhost:0/nosuchrepo.git"

            let cloneButton = findChild(mainWindow, "remoteCloneButton")
            verify(cloneButton !== null)
            mouseClick(cloneButton)

            // Wait for the error to appear.
            let cloneErrorArea = findChild(mainWindow, "remoteCloneErrorArea")
            verify(cloneErrorArea !== null)
            tryVerify(() => cloneErrorArea.visible, 20000)

            // Clone button must be re-enabled so the user can retry.
            verify(cloneButton.enabled)

            // Retry with the valid local bare-remote URL.
            manualUrlField.textField.text = fixture.remoteUrl
            mouseClick(cloneButton)

            // Second clone must succeed — no crash, project opens.
            tryVerify(() => RootData.recentProjectModel.rowCount() === 1, 20000)

            tryVerify(() => {
                return RootData.project.filename !== ""
                       && RootData.project.filename.indexOf(fixture.expectedClonePath) >= 0
            }, 10000)
            compare(RootData.pageSelectionModel.currentPageAddress, "View")
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

        function saveProjectAndGetSyncButton(name) {
            let tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            RootData.newProject()
            tryVerify(() => RootData.project.isTemporaryProject, 5000)
            verify(RootData.project.saveAs(tmpPath + "/" + name + ".cwproj"))
            tryVerify(() => !RootData.project.isTemporaryProject, 5000)
            let syncButton = findChild(mainWindow, "syncButton")
            verify(syncButton !== null)
            tryVerify(() => !syncButton.hasRemote, 5000)
            return syncButton
        }

        function test_syncButton_hasRemote_false_showsCloudUploadIcon() {
            let syncButton = saveProjectAndGetSyncButton("syncbutton-noremote-test")
            verify(syncButton.icon.source.toString().includes("cloud-arrow-up"))
        }

        function test_syncButton_hasRemote_false_tooltip() {
            let syncButton = saveProjectAndGetSyncButton("syncbutton-noremote-tooltip-test")
            verify(syncButton.tooltipText.includes("No remote"))
        }

        function test_syncButton_hasRemote_false_badge_hidden() {
            let syncButton = saveProjectAndGetSyncButton("syncbutton-noremote-badge-test")
            let badge = findChild(syncButton, "statusBadge")
            verify(badge !== null)
            verify(!badge.visible)
        }

        function test_syncButton_hasRemote_false_click_emitsSetupRemoteRequested() {
            let syncButton = saveProjectAndGetSyncButton("syncbutton-noremote-click-test")
            let spy = Qt.createQmlObject(
                'import QtTest; SignalSpy { signalName: "setupRemoteRequested" }',
                rootId)
            spy.target = syncButton
            mouseClick(syncButton)
            compare(spy.count, 1)
            spy.destroy()
        }

        function test_syncButton_hasRemote_false_contextMenu_syncNowDisabled() {
            let syncButton = saveProjectAndGetSyncButton("syncbutton-noremote-menu-test")
            mouseClick(syncButton, syncButton.width / 2, syncButton.height / 2, Qt.RightButton)
            let syncNowItem = findChild(syncButton, "syncNowMenuItem")
            verify(syncNowItem !== null)
            verify(!syncNowItem.enabled)
        }

        // Helper: load a project with a local remote configured.
        function loadSyncFixtureAndGetSyncButton() {
            const fixture = TestHelper.createLocalSyncFixtureWithLfsServer()
            compare(fixture.errorMessage, "")
            TestHelper.loadProjectFromPath(RootData.project, fixture.projectFilePath)
            tryVerify(() => RootData.region.caveCount > 0, 10000,
                      "fixture project should load with caves")
            let syncButton = findChild(mainWindow, "syncButton")
            verify(syncButton !== null)
            tryVerify(() => syncButton.hasRemote, 5000,
                      "sync button should have a remote configured")
            return syncButton
        }

        function test_syncButton_withRemote_projectModified_badgeVisible() {
            let syncButton = loadSyncFixtureAndGetSyncButton()
            let badge = findChild(syncButton, "statusBadge")
            verify(badge !== null)

            // Wait for a clean, non-stale state so we know badge starts hidden
            RootData.project.syncHealth.refresh()
            tryVerify(() => !RootData.project.syncHealth.status.stale
                          && !RootData.project.modified
                          && syncButton.aheadCount === 0
                          && syncButton.behindCount === 0,
                      10000, "wait for clean sync state before testing badge")
            verify(!badge.visible, "badge should be hidden in a clean state")

            RootData.region.cave(0).name = "Modified Cave"
            tryVerify(() => RootData.project.modified, 5000,
                      "project should become modified after cave rename")
            verify(badge.visible, "badge should be visible when project is modified")
        }

        function test_syncButton_withRemote_projectModified_tooltip() {
            let syncButton = loadSyncFixtureAndGetSyncButton()

            RootData.region.cave(0).name = "Modified For Tooltip"
            tryVerify(() => RootData.project.modified, 5000,
                      "project should become modified after cave rename")

            verify(syncButton.tooltipText.includes("Unsaved changes"),
                   "tooltip should mention unsaved changes when project is modified")
        }

        function test_syncButton_withRemote_upToDate_tooltip() {
            let syncButton = loadSyncFixtureAndGetSyncButton()

            RootData.project.syncHealth.refresh()
            tryVerify(() => !RootData.project.syncHealth.status.stale
                          && !RootData.project.modified
                          && syncButton.aheadCount === 0
                          && syncButton.behindCount === 0,
                      10000, "wait for clean sync state")

            verify(syncButton.tooltipText.includes("Up to date"),
                   "tooltip should say 'Up to date' when synced and unmodified")
        }

        function test_syncButton_withRemote_modifiedClears_afterSync() {
            let syncButton = loadSyncFixtureAndGetSyncButton()

            RootData.region.cave(0).name = "Pre-Sync Cave"
            tryVerify(() => RootData.project.modified, 5000,
                      "project should become modified after cave rename")
            verify(syncButton.projectModified,
                   "syncButton.projectModified should reflect project.modified")

            RootData.project.sync()
            tryVerify(() => !RootData.project.syncInProgress, 30000,
                      "sync should complete")

            verify(!RootData.project.modified,
                   "project.modified should be false after successful sync")
            verify(!syncButton.projectModified,
                   "syncButton.projectModified should be false after sync")
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
