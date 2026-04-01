import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQml.Models as QM
import cavewherelib
import QQuickGit

StandardPage {
    id: page
    objectName: "remoteRepositoryPage"

    property GitHubIntegration gitHub: RootData.remote.gitHubIntegration
    property RemoteAccountCoordinator remoteAccountCoordinator: RootData.remote.accountCoordinator
    property AskToSaveDialog askToSaveDialog: null

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
                    active: gitHub.authState === GitHubIntegration.Authorized
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
                    visible: gitHub.authState !== GitHubIntegration.Authorized
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
                        objectName: "remoteManualUrlField"
                        Layout.fillWidth: true

                        textField.placeholderText: "https://github.com/user/repo.git"
                    }

                    QC.Button {
                        id: cloneButton
                        objectName: "remoteCloneButton"
                        text: cloneAreaId.isCloning ? "Cloning..." : "Clone"
                        enabled: manualUrlField.textField.text.length > 0 && !cloneAreaId.isCloning
                        transformOrigin: Item.Center
                        onClicked: cloneAreaId.clone()
                    }
                }

                CloneArea {
                    id: cloneAreaId
                    Layout.fillWidth: true
                    urlText: manualUrlField.textField.text
                    authErrorMessage: qsTr("Select a GitHub account below to clone from GitHub.")
                    onReadyToOpen: function(filePath) {
                        function loadAndView() {
                            RootData.project.loadFile(filePath)
                            RootData.pageSelectionModel.gotoPageByName(null, "View")
                            manualUrlField.textField.text = ""
                        }
                        if (page.askToSaveDialog) {
                            page.askToSaveDialog.taskName = "opening a cloned repository"
                            page.askToSaveDialog.afterSaveFunc = loadAndView
                            page.askToSaveDialog.askToSave()
                        } else {
                            console.error("RemoteRepositoryPage: askToSaveDialog not set; opened cloned project without save prompt.")
                            loadAndView()
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
                objectName: "remoteAccountCombo"
                Layout.fillWidth: true
                textRole: "label"

                model: RemoteAccountSelectionModel {
                    id: accountSelectionModel
                    sourceModel: RootData.remote.accountModel
                    onEntriesChanged: {
                        if (accountCombo.currentIndex < 0 || accountCombo.currentIndex >= accountCombo.count) {
                            accountCombo.currentIndex = 0
                        }
                    }
                }

                // Component.onCompleted: currentIndex = 0

                delegate: Loader {
                    required property int entryType
                    required property int provider
                    required property string label
                    required property string username
                    required property string accountId
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
                        objectName: "remoteUserAccountEntry_" + parent.accountId
                        text: parent.label

                        property int provider: parent.provider

                        enabled: provider !== RemoteAccountModel.Unknown
                        onClicked: {
                            accountCombo.currentIndex = parent.index
                            let selectedUsername = parent.username === undefined ? "" : String(parent.username)
                            if (provider === RemoteAccountModel.GitHub && selectedUsername.length > 0) {
                                remoteAccountCoordinator.selectGitHubAccount(selectedUsername)
                            }
                            page.state = provider === RemoteAccountModel.GitHub ? "githubAccount" : "none"
                            accountCombo.popup.close()
                        }
                        highlighted: hovered
                    }
                }

                Component {
                    id: addEntryDelegate
                    QC.ItemDelegate {
                        objectName: "remoteAddAccountEntry"
                        text: parent.label
                        onClicked: {
                            accountCombo.currentIndex = parent.index
                            page.state = "addAccount"
                            remoteAccountCoordinator.startAddGitHubAccount()
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

            GitHubAuthFlow {
                anchors.left: parent.left
                anchors.right: parent.right
                gitHubIntegration: gitHub
                contextMessage: qsTr("Sign in to GitHub to access your repositories")
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

                    QC.ToolButton {
                        objectName: "remoteRepositoryActionsButton"
                        icon.source: "qrc:/twbs-icons/icons/list.svg"
                        onClicked: repositoryActionsMenu.open()

                        QC.Menu {
                            id: repositoryActionsMenu

                            QC.MenuItem {
                                text: gitHub.busy ? "Refreshing…" : "Refresh"
                                enabled: !gitHub.busy
                                onTriggered: gitHub.refreshRepositories()
                            }

                            QC.MenuSeparator {}

                            QC.MenuItem {
                                objectName: "remoteForgetAccountMenuItem"
                                text: "Forget Account"
                                enabled: !gitHub.busy
                                onTriggered: forgetAccountDialog.open()
                            }
                        }
                    }
                }

                QC.Dialog {
                    id: forgetAccountDialog
                    parent: page.Window.window ? page.Window.window.contentItem : page
                    anchors.centerIn: parent
                    modal: true
                    title: "Forget GitHub Account?"

                    Text {
                        width: 320
                        wrapMode: Text.WordWrap
                        color: Theme.textSecondary
                        text: "This will remove your saved GitHub account from CaveWhere on this device."
                    }

                    footer: QC.DialogButtonBox {
                        standardButtons: QC.DialogButtonBox.Cancel

                        QC.Button {
                            objectName: "remoteForgetAccountConfirmButton"
                            text: "Remove"
                            QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.AcceptRole
                        }
                    }

                    onAccepted: {
                        remoteAccountCoordinator.forgetGitHubAccount()
                        accountCombo.currentIndex = 0
                        page.state = "none"
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
                                manualUrlField.textField.text = cloneUrl
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


}
