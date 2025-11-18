import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib
import QQuickGit

StandardPage {
    id: page

    property int selectedRepoIndex: -1
    property GitHubIntegration gitHub: gitHubLoader.item
    property bool addingGitHubAccount: false
    signal repositoryPicked(string repositoryUrl)

    Loader {
        id: gitHubLoader
        active: false
        sourceComponent: GitHubIntegration {
            id: gitHubInstance

            onAuthStateChanged: page.registerAuthorizedAccount()

            onUsernameChanged: page.registerAuthorizedAccount()

            autoLoadStoredAccount: false
        }
    }

    function withGitHubIntegration(action) {
        if (!gitHubLoader.active) {
            gitHubLoader.active = true
        }

        if (!action) {
            return
        }

        Qt.callLater(function() {
            if (gitHub) {
                action(gitHub)
            }
        })
    }

    function registerAuthorizedAccount() {
        if (!gitHub || gitHub.authState !== GitHubIntegration.Authorized) {
            return
        }
        if (!gitHub.username || gitHub.username.length === 0) {
            return
        }
        addingGitHubAccount = false
        RootData.remoteAccountModel.addOrUpdateAccount(RemoteAccountModel.GitHub, gitHub.username)
    }

    function beginAddAccountFlow() {
        addingGitHubAccount = true
        withGitHubIntegration(function(instance) {
            instance.autoLoadStoredAccount = false
            instance.clearSession()
            instance.startDeviceLogin()
        })
    }

    function activateAccount(provider, username) {
        if (provider !== RemoteAccountModel.GitHub) {
            return
        }

        addingGitHubAccount = false
        withGitHubIntegration(function(instance) {
            if (instance.authState === GitHubIntegration.Authorized) {
                instance.refreshRepositories()
            } else {
                instance.autoLoadStoredAccount = true
            }
        })
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Text {
            text: "Connect to a Remote Caving Area"
            font.pixelSize: 20
            Layout.fillWidth: true
        }

        // Text {
        //     text: "Sign in with GitHub to pick from your repositories or paste a repository URL to clone directly."
        //     wrapMode: Text.WordWrap
        //     color: "#666666"
        //     Layout.fillWidth: true
        // }


        QC.GroupBox {
            Layout.fillWidth: true
            title: "Clone from HTTP/SSH"

            RowLayout {
                // anchors.fill: parent
                // Layout.fillWidth: true
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                spacing: 8

                TextFieldWithError {
                    id: manualUrlField
                    // width: 100
                    Layout.fillWidth: true
                    textField.placeholderText: "https://github.com/user/repo.git"
                }

                // Item { Layout.fillWidth: true }

                QC.Button {
                    id: cloneButton
                    text: "Clone"
                    enabled: manualUrlField.textField.text.length > 0
                    transformOrigin: Item.Center
                    onClicked: {
                        console.log("TODO: clone", manualUrlField.textField.text)
                    }
                }
            }
        }

        QC.GroupBox {
            Layout.fillWidth: true
            title: "GitHub"
            visible: gitHubLoader.active
                     && gitHub
                     && gitHub.authState !== GitHubIntegration.Authorized
                     && addingGitHubAccount

            ColumnLayout {
                // anchors.fill: parent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    color: "#444444"
                    text: {
                        if (!gitHub) {
                            return "Use your GitHub account to access remote repositories."
                        }
                        switch (gitHub.authState) {
                        case GitHubIntegration.RequestingCode:
                            return "Requesting a sign-in code from GitHub…";
                        case GitHubIntegration.AwaitingVerification:
                            return "Enter the code below at GitHub to finish signing in.";
                        case GitHubIntegration.Error:
                            return gitHub.errorMessage.length > 0 ? gitHub.errorMessage : "Something went wrong.";
                        default:
                            return "Use your GitHub account to access remote repositories.";
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: gitHub && gitHub.authState === GitHubIntegration.AwaitingVerification

                    QC.TextField {
                        Layout.fillWidth: true
                        text: gitHub ? gitHub.userCode : ""
                        readOnly: true
                        horizontalAlignment: Text.AlignHCenter
                        font.bold: true
                    }

                    QC.Button {
                        text: "Copy and Open GitHub"
                        onClicked: {
                            if (gitHub && gitHub.userCode && gitHub.userCode.length > 0) {
                                RootData.copyText(gitHub.userCode)
                            }
                            if (gitHub) {
                                gitHub.markVerificationOpened()
                                Qt.openUrlExternally(gitHub.verificationUrl)
                            }
                        }
                        // QC.ToolTip.visible: hovered
                        // QC.ToolTip.text: "Copy the code and open github.com/device"
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: gitHub
                              && gitHub.authState === GitHubIntegration.AwaitingVerification
                              && gitHub.verificationOpened
                              && gitHub.secondsUntilNextPoll > 0
                    color: "#666666"
                    font.pixelSize: 12
                    text: gitHub ? qsTr("Trying connection in %1 s").arg(gitHub.secondsUntilNextPoll) : ""
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: gitHub && gitHub.authState !== GitHubIntegration.AwaitingVerification

                    QC.Button {
                        text: gitHub && gitHub.authState === GitHubIntegration.Error ? "Try Again" : "Connect to GitHub"
                        enabled: gitHub && !gitHub.busy
                        onClicked: gitHub && gitHub.startDeviceLogin()
                    }

                    QC.Button {
                        text: "Cancel"
                        visible: gitHub
                                 && (gitHub.authState === GitHubIntegration.AwaitingVerification
                                     || gitHub.authState === GitHubIntegration.RequestingCode)
                        onClicked: {
                            addingGitHubAccount = false
                            if (gitHub) {
                                gitHub.cancelLogin()
                            }
                        }
                    }
                }
            }
        }


        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "Account:"
            }

            QC.ComboBox {
                id: accountCombo
                Layout.fillWidth: true
                textRole: "label"

                model: RemoteAccountSelectionModel {
                    id: accountSelectionModel
                    sourceModel: RootData.remoteAccountModel
                    onEntriesChanged: accountCombo.currentIndex = 0
                }

                // Component.onCompleted: currentIndex = 0

                delegate: Loader {
                    required property int entryType
                    required property int provider
                    required property string label
                    required property string username
                    required property int index

                    width: accountCombo.width
                    // sourceComponent: separatorDelegate
                    sourceComponent: entryType === RemoteAccountSelectionModel.SeparatorEntry ? separatorDelegate : accountEntryDelegate
                }

                Component {
                    id: accountEntryDelegate
                    QC.ItemDelegate {
                        text: parent.label

                        property int entryType: parent.entryType
                        property int provider: parent.provider
                        property string username: parent.username

                        enabled: entryType !== RemoteAccountSelectionModel.AccountEntry || provider !== RemoteAccountModel.Unknown
                        onClicked: {
                            accountCombo.currentIndex = parent.index

                            if (entryType === RemoteAccountSelectionModel.NoneEntry) {
                                page.addingGitHubAccount = false
                                return
                            } else if (entryType === RemoteAccountSelectionModel.AddEntry) {
                                page.beginAddAccountFlow()
                            } else if (entryType === RemoteAccountSelectionModel.AccountEntry) {
                                if (provider === RemoteAccountModel.GitHub) {
                                    page.activateAccount(provider, username)
                                }
                            }
                            accountCombo.popup.close()
                        }

                        highlighted: hovered
                    }
                }

                Component {
                    id: separatorDelegate

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 5
                        height: 1
                        color: "black"
                    }
                }
            }
        }

        Loader {
            active: gitHub && gitHub.authState === GitHubIntegration.Authorized
            Layout.fillWidth: true
            Layout.fillHeight: true

            sourceComponent: ColumnLayout {
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Repositories"
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    QC.Button {
                        text: gitHub && gitHub.busy ? "Refreshing…" : "Refresh"
                        enabled: gitHub && !gitHub.busy
                        onClicked: gitHub && gitHub.refreshRepositories()
                    }

                    QC.Button {
                        text: "Upload SSH Key"
                        enabled: gitHub && !gitHub.busy
                        onClicked: gitHub && gitHub.uploadPublicKey("")
                    }

                    QC.Button {
                        text: "Logout"
                        enabled: gitHub && !gitHub.busy
                        onClicked: gitHub && gitHub.logout()
                    }
                }

                ListView {
                    id: repoList
                    currentIndex: page.selectedRepoIndex
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // Layout.preferredHeight: 240
                    clip: true
                    model: gitHub ? gitHub.repositories : []
                    QC.ScrollBar.vertical: QC.ScrollBar {
                        policy: QC.ScrollBar.AsNeeded
                    }
                    onCountChanged: {
                        if (page.selectedRepoIndex >= count) {
                            page.selectedRepoIndex = -1
                        }
                    }

                    delegate: Rectangle {
                        required property var modelData
                        required property int index

                        width: repoList.width
                        height: layoutId.height + 10
                        color: page.selectedRepoIndex === index
                               ? "#d6ecff"
                               : (index % 2 === 0 ? "#ffffff" : "#f7f7f7")

                        RowLayout {
                            id: layoutId
                            anchors.left: parent.left
                            anchors.right: parent.right

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: 8
                                Layout.rightMargin: 8
                                Layout.topMargin: 8
                                Layout.bottomMargin: 8
                                spacing: 4

                                RowLayout {
                                    Text {
                                        text: modelData.name
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: modelData.isPrivate ? "Private" : "Public"
                                        color: modelData.isPrivate ? "#b85600" : "#2e7d32"
                                        font.pixelSize: 12
                                    }
                                }

                                Text {
                                    text: modelData.description.length > 0 ? modelData.description : modelData.cloneUrl
                                    wrapMode: Text.WordWrap
                                    elide: Text.ElideRight
                                    color: "#666666"
                                }
                            }

                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            onTapped: {
                                page.selectedRepoIndex = index
                                page.repositoryPicked(modelData.sshUrl)
                            }
                        }
                    }

                    footer: Item {
                        width: repoList.width
                        height: gitHub && gitHub.repositories.length === 0 ? 40 : 0

                        Text {
                            anchors.centerIn: parent
                            text: "No repositories found."
                            visible: gitHub && gitHub.repositories.length === 0
                            color: "#666666"
                        }
                    }
                }
            }
        }



        // Text {
        //     Layout.fillWidth: true
        //     visible: gitHub.errorMessage.length > 0 && gitHub.authState === GitHubIntegration.Authorized
        //     wrapMode: Text.WordWrap
        //     color: "#b00020"
        //     text: gitHub.errorMessage
        // }

        // Item { Layout.fillHeight: true }
    }

    SequentialAnimation {
        id: cloneButtonPulse
        loops: 2
        running: false
        NumberAnimation {
            target: cloneButton
            property: "scale"
            from: 1.0
            to: 1.15
            duration: 140
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            target: cloneButton
            property: "scale"
            from: 1.15
            to: 1.0
            duration: 220
            easing.type: Easing.InOutQuad
        }
        onStopped: cloneButton.scale = 1.0
    }

    onRepositoryPicked: {
        manualUrlField.textField.text = repositoryUrl
        manualUrlField.textField.focus = true
        manualUrlField.textField.selectAll()
        cloneButtonPulse.restart()
    }
}
