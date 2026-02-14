import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQml.Models as QM
import cavewherelib
import QQuickGit

StandardPage {
    id: page

    property GitHubIntegration gitHub: RootData.gitHubIntegration
    state: "none"
    states: [
        State {
            name: "none"
            changes: [
                PropertyChanges {
                    target: gitHub
                    active: false
                },
                PropertyChanges {
                    target: gitHubGroup
                    visible: false
                },
                PropertyChanges {
                    target: repositoryLoader
                    active: false
                },
                StateChangeScript {
                    script: gitHub.cancelDeviceLoginFlow()
                }
            ]
        },
        State {
            name: "addAccount"
            changes: [
                PropertyChanges {
                    target: gitHub
                    active: true
                },
                PropertyChanges {
                    target: gitHubGroup
                    visible: gitHub.authState !== GitHubIntegration.Authorized
                },
                PropertyChanges {
                    target: repositoryLoader
                    active: false
                }
            ]
        },
        State {
            name: "githubAccount"
            changes: [
                PropertyChanges {
                    target: gitHub
                    active: true
                },
                PropertyChanges {
                    target: gitHubGroup
                    visible: false
                },
                PropertyChanges {
                    target: repositoryLoader
                    active: gitHub.authState === GitHubIntegration.Authorized
                },
                StateChangeScript {
                    script: {
                        gitHub.cancelDeviceLoginFlow()
                        if (gitHub.authState === GitHubIntegration.Authorized) {
                            gitHub.refreshRepositories()
                        }
                    }
                }
            ]
        }
    ]
    transitions: [
        Transition {
            from: "githubAccount"
            ScriptAction {
                script: gitHub.cancelDeviceLoginFlow()
            }
        }
    ]

    readonly property url cloneDestinationParentFolder: {
        if (cloneDestinationDialog.selectedFolder.toString() !== "") {
            return cloneDestinationDialog.selectedFolder
        }
        if (cloneDestinationDialog.currentFolder.toString() !== "") {
            return cloneDestinationDialog.currentFolder
        }
        return RootData.repositoryModel.defaultRepositoryDir
    }
    readonly property string cloneDestinationRepoName: remoteRepositoryCloner.repositoryNameFromUrl(manualUrlField.textField.text)
    readonly property string cloneDestinationPathText: {
        const parentPath = RootData.urlToLocal(cloneDestinationParentFolder)
        if (parentPath.length === 0) {
            return repoName
        }
        if(cloneDestinationRepoName.length === 0) {
            return parentPath
        }

        const separator = parentPath.endsWith("/") || parentPath.endsWith("\\") ? "" : "/"
        return parentPath + separator + cloneDestinationRepoName
    }

    RemoteRepositoryCloner {
        id: remoteRepositoryCloner
        repositoryModel: RootData.repositoryModel
        cloneWatcher: cloneWatcher
        account: RootData.account
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
        //     color: Theme.textSubtle
        //     Layout.fillWidth: true
        // }


        QC.GroupBox {
            Layout.fillWidth: true
            title: "Clone from HTTP/SSH"

            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true

                    TextFieldWithError {
                        id: manualUrlField
                        Layout.fillWidth: true

                        textField.placeholderText: "https://github.com/user/repo.git"
                    }

                    QC.Button {
                        id: cloneButton
                        text: cloneWatcher.state === GitFutureWatcher.Loading ? "Cloning..." : "Clone"
                        enabled: manualUrlField.textField.text.length > 0
                                 && cloneWatcher.state !== GitFutureWatcher.Loading
                        transformOrigin: Item.Center
                        onClicked: {
                            remoteRepositoryCloner.clone(manualUrlField.textField.text, page.cloneDestinationParentFolder)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Destination:"
                        color: Theme.textSubtle
                    }

                    LinkText {
                        Layout.fillWidth: true
                        elide: Text.ElideLeft
                        text: page.cloneDestinationPathText
                        onClicked: {
                            cloneDestinationDialog.open()
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: cloneWatcher.state === GitFutureWatcher.Loading
                     || remoteRepositoryCloner.cloneErrorMessage.length > 0
                     || remoteRepositoryCloner.cloneStatusMessage.length > 0

            QC.ProgressBar {
                Layout.fillWidth: true
                visible: cloneWatcher.state === GitFutureWatcher.Loading
                from: 0
                to: 1
                value: cloneWatcher.progress
            }

            Text {
                Layout.fillWidth: true
                visible: remoteRepositoryCloner.cloneStatusMessage.length > 0
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                color: Theme.textSecondary
                text: remoteRepositoryCloner.cloneStatusMessage
            }

            ErrorHelpArea {
                Layout.fillWidth: true
                text: remoteRepositoryCloner.cloneErrorMessage
                visible: remoteRepositoryCloner.cloneErrorMessage.length > 0
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
                    onEntriesChanged: {
                        accountCombo.currentIndex = 0
                        page.state = "none"
                    }
                }

                // Component.onCompleted: currentIndex = 0

                delegate: Loader {
                    required property int entryType
                    required property int provider
                    required property string label
                    required property int index

                    width: accountCombo.width
                    sourceComponent: {
                        switch (entryType) {
                        case RemoteAccountSelectionModel.SeparatorEntry:
                            return separatorDelegate
                        case RemoteAccountSelectionModel.NoneEntry:
                            return noneEntryDelegate
                        case RemoteAccountSelectionModel.AddEntry:
                            return addEntryDelegate
                        default:
                            return userAccountEntryDelegate
                        }
                    }
                }

                Component {
                    id: noneEntryDelegate
                    QC.ItemDelegate {
                        text: parent.label

                        onClicked: {
                            accountCombo.currentIndex = parent.index
                            page.state = "none"
                            accountCombo.popup.close()
                        }
                        highlighted: hovered
                    }
                }

                Component {
                    id: userAccountEntryDelegate
                    QC.ItemDelegate {
                        text: parent.label

                        property int provider: parent.provider

                        enabled: provider !== RemoteAccountModel.Unknown
                        onClicked: {
                            accountCombo.currentIndex = parent.index
                            page.state = provider === RemoteAccountModel.GitHub ? "githubAccount" : "none"
                            accountCombo.popup.close()
                        }
                        highlighted: hovered
                    }
                }

                Component {
                    id: addEntryDelegate
                    QC.ItemDelegate {
                        text: parent.label
                        onClicked: {
                            accountCombo.currentIndex = parent.index
                            page.state = "addAccount"
                            gitHub.startDeviceLogin()
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
                        color: Theme.divider
                    }
                }
            }
        }


        QC.GroupBox {
            id: gitHubGroup
            Layout.fillWidth: true
            title: "GitHub"
            visible: false

            ColumnLayout {
                // anchors.fill: parent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    color: Theme.textSecondary
                    text: {
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
                    visible: gitHub.authState === GitHubIntegration.AwaitingVerification

                    QC.TextField {
                        Layout.fillWidth: true
                        text: gitHub.userCode
                        readOnly: true
                        horizontalAlignment: Text.AlignHCenter
                        font.bold: true
                    }

                    QC.Button {
                        text: "Copy and Open GitHub"
                        onClicked: {
                            if (gitHub.userCode && gitHub.userCode.length > 0) {
                                RootData.copyText(gitHub.userCode)
                            }
                            gitHub.markVerificationOpened()
                            Qt.openUrlExternally(gitHub.verificationUrl)
                        }
                        // QC.ToolTip.visible: hovered
                        // QC.ToolTip.text: "Copy the code and open github.com/device"
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: gitHub.authState === GitHubIntegration.AwaitingVerification
                              && gitHub.verificationOpened
                              && gitHub.secondsUntilNextPoll > 0
                    color: Theme.textSubtle
                    font.pixelSize: 12
                    text: qsTr("Trying connection in %1 s").arg(gitHub.secondsUntilNextPoll)
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: gitHub.authState !== GitHubIntegration.AwaitingVerification

                    QC.Button {
                        text: gitHub.authState === GitHubIntegration.Error ? "Try Again" : "Connect to GitHub"
                        enabled: !gitHub.busy
                        onClicked: gitHub.startDeviceLogin()
                    }

                    QC.Button {
                        text: "Cancel"
                        visible: gitHub.authState === GitHubIntegration.AwaitingVerification || gitHub.authState === GitHubIntegration.RequestingCode
                        onClicked: gitHub.cancelLogin()
                    }
                }
            }
        }

        Loader {
            id: repositoryLoader
            active: false
            Layout.fillWidth: true
            Layout.fillHeight: true

            sourceComponent: ColumnLayout {
                spacing: 12

                QM.SortFilterProxyModel {
                    id: repositorySortFilterModel
                    model: gitHub.repositories
                    sorters: [
                        QM.StringSorter {
                            roleName: "name"
                            sortOrder: Qt.AscendingOrder
                            caseSensitivity: Qt.CaseInsensitive
                        }
                    ]
                    filters: [
                        QM.FunctionFilter {
                            component RoleData: QtObject {
                                property string name
                            }

                            function filter(data: RoleData): bool {
                                const normalizedFilter = repositoryFilterField.text.trim().toLowerCase()
                                if (normalizedFilter.length === 0) {
                                    return true
                                }
                                return data.name.toLowerCase().indexOf(normalizedFilter) !== -1
                            }
                        }
                    ]
                }

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Repositories"
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    QC.Button {
                        text: gitHub.busy ? "Refreshing…" : "Refresh"
                        enabled: !gitHub.busy
                        onClicked: gitHub.refreshRepositories()
                    }

                    QC.Button {
                        text: "Upload SSH Key"
                        enabled: !gitHub.busy
                        onClicked: gitHub.uploadPublicKey("")
                    }

                    QC.Button {
                        text: "Logout"
                        enabled: !gitHub.busy
                        onClicked: gitHub.logout()
                    }
                }

                QC.TextField {
                    id: repositoryFilterField
                    Layout.fillWidth: true
                    placeholderText: "Filter repositories by name"
                    onTextChanged: {
                        repoList.currentIndex = -1
                        repositorySortFilterModel.invalidate()
                    }
                }

                ListView {
                    id: repoList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // Layout.preferredHeight: 240
                    clip: true
                    model: repositorySortFilterModel
                    QC.ScrollBar.vertical: QC.ScrollBar {
                        policy: QC.ScrollBar.AsNeeded
                    }
                    onCountChanged: {
                        if (repoList.currentIndex >= count) {
                            repoList.currentIndex = -1
                        }
                    }

                    delegate: Rectangle {
                        required property string name
                        required property bool isPrivate
                        required property string description
                        required property string cloneUrl
                        required property string sshUrl
                        required property int index

                        width: repoList.width
                        height: layoutId.height + 10
                        color: repoList.currentIndex === index
                               ? Theme.highlight
                               : (index % 2 === 0 ? Theme.surface : Theme.surfaceMuted)

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
                                        text: name
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: isPrivate ? "Private" : "Public"
                                        color: isPrivate ? Theme.warning : Theme.success
                                        font.pixelSize: 12
                                    }
                                }

                                Text {
                                    text: description.length > 0 ? description : cloneUrl
                                    wrapMode: Text.WordWrap
                                    elide: Text.ElideRight
                                    color: Theme.textSubtle
                                }
                            }

                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            onTapped: {
                                repoList.currentIndex = index
                                manualUrlField.textField.text = sshUrl
                                manualUrlField.textField.focus = true
                                manualUrlField.textField.selectAll()
                                cloneButtonPulse.restart()
                            }
                        }
                    }

                    footer: Item {
                        width: repoList.width
                        height: repoList.count === 0 ? 40 : 0

                        Text {
                            anchors.centerIn: parent
                            text: gitHub.busy ? "Refreshing repositories..." : "No repositories found."
                            visible: repoList.count === 0
                            color: Theme.textSubtle
                        }
                    }
                }
            }
        }



        // Text {
        //     Layout.fillWidth: true
        //     visible: gitHub.errorMessage.length > 0 && gitHub.authState === GitHubIntegration.Authorized
        //     wrapMode: Text.WordWrap
        //     color: Theme.danger
        //     text: gitHub.errorMessage
        // }

        // Item { Layout.fillHeight: true }
    }

    FolderDialog {
        id: cloneDestinationDialog
        currentFolder: RootData.repositoryModel.defaultRepositoryDir
        selectedFolder: currentFolder
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

    GitFutureWatcher {
        id: cloneWatcher
        initialProgressText: "Cloning"
    }

}
