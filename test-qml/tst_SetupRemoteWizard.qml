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

    CWTestCase {
        name: "SetupRemoteWizard"
        when: windowShown

        property var wizard: null

        function init() {
            RootData.newProject()
            tryVerify(() => RootData.project.isTemporaryProject, 5000)
            let tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tmpPath + "/wizard-test.cwproj"))
            tryVerify(() => !RootData.project.isTemporaryProject, 5000)
        }

        function cleanup() {
            if (wizard !== null) {
                wizard.close()
                wizard.destroy()
                wizard = null
            }
        }

        function openWizard() {
            let comp = Qt.createComponent("qrc:/qt/qml/cavewherelib/qml/SetupRemoteWizard.qml")
            verify(comp.status === Component.Ready, "SetupRemoteWizard.qml failed to load: " + comp.errorString())
            wizard = comp.createObject(rootId, {
                gitHubIntegration: RootData.remote.gitHubIntegration,
                accountCoordinator: RootData.remote.accountCoordinator,
                repository: RootData.project.repository
            })
            verify(wizard !== null, "Failed to create SetupRemoteWizard")
            wizard.open()
            tryVerify(() => wizard.visible, 5000)
            return wizard
        }

        function openWizardWithRepository() {
            // Wait for repository to be initialized (async after saveAs)
            tryVerify(() => RootData.project.repository !== null, 5000)
            return openWizard()
        }

        // ── Screen navigation ──────────────────────────────────────────────

        function test_opensOnCreateRepoScreen() {
            let w = openWizard()
            compare(w.currentState, "createRepo")
        }

        function test_alreadyHaveRemoteLink_navigatesToConnectScreen() {
            let w = openWizard()
            let link = findChild(w, "alreadyHaveRemoteLink")
            verify(link !== null)
            mouseClick(link)
            tryVerify(() => w.currentState === "connectRepo", 2000)
        }

        function test_backLink_returnsToCreateScreen() {
            let w = openWizard()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo", 2000)
            mouseClick(findChild(w, "createInsteadLink"))
            tryVerify(() => w.currentState === "createRepo", 2000)
        }

        // ── Path A form guards ─────────────────────────────────────────────

        function test_pathA_createButtonDisabledWhenNameEmpty() {
            let w = openWizard()
            compare(w.currentState, "createRepo")
            // Create button only visible when authorized; check the form's disabled state
            // When not authorized the form is hidden — skip if not authorized
            if (RootData.remote.gitHubIntegration.authState !== GitHubIntegration.Authorized) {
                skip("Not authorized — Create form is hidden")
                return
            }
            let nameField = findChild(w, "repoNameField")
            verify(nameField !== null)
            nameField.text = ""
            let createButton = findChild(w, "createButton")
            verify(createButton !== null)
            verify(!createButton.enabled)
        }

        // ── Path C (custom URL) — fully automated with local bare repo ─────

        function test_pathC_connectButtonDisabledWhenUrlEmpty() {
            let w = openWizard()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo", 2000)
            mouseClick(findChild(w, "customUrlTab"))
            let connectButton = findChild(w, "connectButton")
            verify(connectButton !== null)
            verify(!connectButton.enabled)
        }

        function test_pathC_connectButtonEnabledWhenUrlFilled() {
            let w = openWizard()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo", 2000)
            mouseClick(findChild(w, "customUrlTab"))
            let urlField = findChild(w, "customUrlField")
            verify(urlField !== null)
            urlField.text = "https://example.com/repo.git"
            tryVerify(() => findChild(w, "connectButton").enabled, 2000)
        }

        function test_pathC_connectLocalBareRepo_reachesDoneState() {
            let fixture = TestHelper.createLocalBareRemoteForCloneTest()
            compare(fixture.errorMessage, "")

            let w = openWizardWithRepository()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo", 2000)
            mouseClick(findChild(w, "customUrlTab"))
            findChild(w, "customUrlField").text = fixture.remoteUrl
            tryVerify(() => findChild(w, "connectButton").enabled, 2000)
            mouseClick(findChild(w, "connectButton"))

            tryVerify(() => w.currentState === "done", 10000)
        }

        function test_doneScreen_showsSyncNowAndCloseButtons() {
            let fixture = TestHelper.createLocalBareRemoteForCloneTest()
            compare(fixture.errorMessage, "")

            let w = openWizardWithRepository()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo", 2000)
            mouseClick(findChild(w, "customUrlTab"))
            findChild(w, "customUrlField").text = fixture.remoteUrl
            tryVerify(() => findChild(w, "connectButton").enabled, 2000)
            mouseClick(findChild(w, "connectButton"))
            tryVerify(() => w.currentState === "done", 10000)

            let syncNowButton = findChild(w, "syncNowButton")
            let closeButton = findChild(w, "closeButton")
            verify(syncNowButton !== null && syncNowButton.visible)
            verify(closeButton !== null && closeButton.visible)
        }

        function test_pathC_badUrl_errorShown_staysOnConnectScreen() {
            // Pre-occupy "origin" so the coordinator's addRemoteToProject reliably fails
            let fixture = TestHelper.createLocalBareRemoteForCloneTest()
            compare(fixture.errorMessage, "")
            tryVerify(() => RootData.project.repository !== null, 5000)
            let preAddError = RootData.project.repository.addRemote("origin", fixture.remoteUrl)
            compare(preAddError, "")

            let w = openWizardWithRepository()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo", 2000)
            mouseClick(findChild(w, "customUrlTab"))
            findChild(w, "customUrlField").text = fixture.remoteUrl
            tryVerify(() => findChild(w, "connectButton").enabled, 2000)
            mouseClick(findChild(w, "connectButton"))

            tryVerify(() => w.connectForm.errorMessage !== "", 5000)
            compare(w.currentState, "connectRepo")
        }

        // ── Path A client-side duplicate-name check ────────────────────────

        function test_pathA_duplicateName_inlineError_noApiCall() {
            if (RootData.remote.gitHubIntegration.authState !== GitHubIntegration.Authorized) {
                skip("Not authorized — duplicate name check requires a populated repo list")
                return
            }
            RootData.remote.gitHubIntegration.refreshRepositories()
            tryVerify(() => RootData.remote.gitHubIntegration.repositories.rowCount() > 0, 15000)

            let existingName = RootData.remote.gitHubIntegration.repositories
                .data(RootData.remote.gitHubIntegration.repositories.index(0, 0), Qt.DisplayRole)

            let w = openWizard()
            compare(w.currentState, "createRepo")

            let nameField = findChild(w, "repoNameField")
            nameField.text = existingName

            let createButton = findChild(w, "createButton")
            mouseClick(createButton)

            tryVerify(() => w.createForm.errorMessage !== "", 2000)
            compare(w.currentState, "createRepo")
        }

        // ── Tester-assisted: Path A requires live GitHub ───────────────────

        function test_testerAssisted_pathA_createRepo_reachesDone() {
            testerAssistedGate.beginDecision(
                "Path A: Create GitHub repo",
                "Requires a GitHub account.\n"
                + "Sign in when prompted, then confirm repo creation completes.")
            tryVerify(() => testerAssistedGate.decisionReady, testerAssistedGate.decisionTimeoutMs)
            if (!testerAssistedGate.runCurrentTest) {
                skip("Tester-assisted test skipped.")
                return
            }

            let w = openWizard()
            if (RootData.remote.gitHubIntegration.authState !== GitHubIntegration.Authorized) {
                tryVerify(() => findChild(w, "copyAndOpenButton") !== null, 10000)
                mouseClick(findChild(w, "copyAndOpenButton"))
                tryVerify(() => RootData.remote.gitHubIntegration.authState
                                === GitHubIntegration.Authorized, 300000)
            }

            let createButton = findChild(w, "createButton")
            tryVerify(() => createButton !== null && createButton.enabled, 10000)
            mouseClick(createButton)
            tryVerify(() => w.currentState === "done", 30000)
        }
    }
}
