import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib
import QQuickGit

QC.Dialog {
    id: root

    required property GitHubIntegration gitHubIntegration
    required property RemoteAccountCoordinator accountCoordinator
    property GitRepository repository: null

    signal remoteSetupComplete(bool syncNow)
    signal remoteSetupCancelled()

    property string currentState: "createRepo"

    // Expose forms for test inspection
    readonly property alias createForm: createFormId
    readonly property alias connectForm: connectFormId

    // Private: tracks which screen to return to on addRemote failure
    property string _addRemoteReturnScreen: "createRepo"
    property bool _addRemotePending: false
    property bool _gitHubWasActive: false

    title: qsTr("Set Up Remote")
    modal: true
    width: 480
    standardButtons: QC.Dialog.NoButton

    onOpened: {
        root.currentState = "createRepo"
        root._addRemotePending = false
        createFormId.errorMessage = ""
        connectFormId.errorMessage = ""

        let regionName = RootData.region.name.trim()
        if (regionName.length > 0) {
            // Sanitize for GitHub: spaces → hyphens, strip chars not in [a-zA-Z0-9._-]
            createFormId.repoName = regionName.replace(/\s+/g, "-")
                                              .replace(/[^a-zA-Z0-9._-]/g, "")
        }

        // Load cached credentials eagerly so startDeviceLogin() only initiates OAuth,
        // avoiding a race between the keychain read and the OAuth device flow.
        root._gitHubWasActive = root.gitHubIntegration.active
        root.gitHubIntegration.active = true

        if (root.gitHubIntegration.authState === GitHubIntegration.Authorized) {
            root.gitHubIntegration.refreshRepositories()
        }
    }

    onClosed: {
        root.gitHubIntegration.active = root._gitHubWasActive
    }

    onRejected: root.remoteSetupCancelled()

    function _addRemote(remoteUrl, bindToGitHub, returnScreen) {
        if (!root.repository) {
            console.warn("SetupRemoteWizard: repository is null")
            return
        }
        root._addRemoteReturnScreen = returnScreen
        root._addRemotePending = true
        root.accountCoordinator.addRemoteToProject(root.repository, remoteUrl, bindToGitHub)
        if (root._addRemotePending) {
            root._addRemotePending = false
            root.currentState = "done"
        }
        // else: addRemoteFailed fired synchronously → state already set back
    }

    Connections {
        target: root.gitHubIntegration

        function onRepositoryCreated(repo) {
            root._addRemote(repo.cloneUrl, true, "createRepo")
        }

        function onRepositoryCreationFailed(errorMessage) {
            root.currentState = "createRepo"
            createFormId.errorMessage = errorMessage
        }
    }

    Connections {
        target: root.accountCoordinator

        function onAddRemoteFailed(errorMessage) {
            if (!root._addRemotePending) { return }
            root._addRemotePending = false
            root.currentState = root._addRemoteReturnScreen
            if (root._addRemoteReturnScreen === "createRepo") {
                createFormId.errorMessage = errorMessage
            } else {
                connectFormId.errorMessage = errorMessage
            }
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 12

        // ── Screen 1: Create on GitHub ─────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.currentState === "createRepo"
            spacing: 8

            // Auth flow shown until authorized
            GitHubAuthFlow {
                Layout.fillWidth: true
                visible: root.gitHubIntegration.authState !== GitHubIntegration.Authorized
                gitHubIntegration: root.gitHubIntegration
                contextMessage: qsTr("Sign in to GitHub to create a repository for this project")
            }

            // Create form shown once authorized
            CreateGitHubRepoForm {
                id: createFormId
                Layout.fillWidth: true
                visible: root.gitHubIntegration.authState === GitHubIntegration.Authorized
                gitHubIntegration: root.gitHubIntegration

                onCreateRequested: function(repoName, isPrivate) {
                    root.currentState = "working"
                    root.gitHubIntegration.createRepository(repoName, isPrivate)
                }
            }

            LinkText {
                objectName: "alreadyHaveRemoteLink"
                text: qsTr("Already have a remote? →")
                onClicked: root.currentState = "connectRepo"
            }
        }

        // ── Screen 2: Connect existing remote ─────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.currentState === "connectRepo"
            spacing: 8

            LinkText {
                objectName: "createInsteadLink"
                text: qsTr("← Create instead")
                onClicked: root.currentState = "createRepo"
            }

            ConnectRemoteForm {
                id: connectFormId
                Layout.fillWidth: true
                gitHubIntegration: root.gitHubIntegration

                onConnectRequested: function(remoteUrl, bindToGitHubAccount) {
                    root._addRemote(remoteUrl, bindToGitHubAccount, "connectRepo")
                }
            }
        }

        // ── Working screen ────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.currentState === "working"
            spacing: 8

            QC.BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: root.currentState === "working"
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Configuring remote…")
                color: Theme.textSecondary
            }

            QC.Button {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Cancel")
                onClicked: {
                    root.currentState = "createRepo"
                    root._addRemotePending = false
                    root.remoteSetupCancelled()
                }
            }
        }

        // ── Done screen ───────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.currentState === "done"
            spacing: 8

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                text: qsTr("Remote configured. Push your project to GitHub to back it up.")
                color: Theme.textSecondary
            }

            RowLayout {
                Layout.fillWidth: true

                QC.Button {
                    objectName: "syncNowButton"
                    text: qsTr("Sync now")
                    onClicked: root.remoteSetupComplete(true)
                }

                Item { Layout.fillWidth: true }

                QC.Button {
                    objectName: "closeButton"
                    text: qsTr("Close")
                    onClicked: root.remoteSetupComplete(false)
                }
            }
        }
    }
}
