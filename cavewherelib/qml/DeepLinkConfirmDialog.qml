pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    id: rootId

    // Emitted when a clone completes and the caller should ask-to-save then load the file.
    signal openRequested(string filePath)

    // Accepts a string or QUrl; SSH URLs must be passed as strings because
    // QUrl parses `git@host:user/repo.git` to empty.
    function open(url) {
        dialogId.repoUrl = typeof url === "string" ? url : url.toString()
        dialogId._selectedUsername = ""
        dialogId._addingAccount = false
        cloneAreaId.reset()
        dialogId._autoSelectAccount()
        dialogId.open()
    }

    function close() {
        dialogId.close()
    }

    QC.Dialog {
        id: dialogId
        objectName: "deepLinkConfirmDialog"

        property string repoUrl
        property string _selectedUsername: ""
        property bool _addingAccount: false

        // SSH URLs authenticate with a local key, not a GitHub token — hide
        // the account picker and auth flow when the URL is SSH.
        readonly property bool _isSshUrl: repoUrl.length > 0 && cloneAreaId.isSshUrl(repoUrl)

        readonly property GitHubIntegration _gitHub: RootData.remote.gitHubIntegration

        function _autoSelectAccount() {
            for (var i = 0; i < accountCombo.count; i++) {
                const idx = accountSelectionModel.index(i, 0)
                const entryType = accountSelectionModel.data(idx, RemoteAccountSelectionModel.EntryTypeRole)
                if (entryType === RemoteAccountSelectionModel.AccountEntry) {
                    accountCombo.currentIndex = i
                    _selectedUsername = accountSelectionModel.data(idx, RemoteAccountSelectionModel.UsernameRole) ?? ""
                    // Load the keychain token now so the subsequent Clone click has
                    // credentials available. rootId.open() calls cloneAreaId.reset()
                    // before _autoSelectAccount(), which clears cloneFailedDueToAuthError,
                    // so the cloner's token-arrival auto-retry cannot fire here.
                    _activateSelectedAccount()
                    return
                }
            }
            accountCombo.currentIndex = 0
        }

        function _activateSelectedAccount() {
            if (_selectedUsername.length === 0) return
            RootData.remote.accountCoordinator.selectGitHubAccount(_selectedUsername)
            _gitHub.active = true
        }

        // Show the inline auth flow when adding an account or after an auth error,
        // but not when already authorized (token loaded successfully)
        readonly property bool _showAuthFlow: {
            if (_isSshUrl) return false
            if (_gitHub.authState === GitHubIntegration.Authorized) return false
            return _addingAccount || cloneAreaId.cloneFailedDueToAuthError
        }

        readonly property string _repoDisplay: {
            const str = repoUrl
            if (str === "") return ""
            try {
                const u = new URL(str)
                return u.hostname + " / " + u.pathname.replace(/^\//, "")
            } catch(e) {
                return str
            }
        }

        anchors.centerIn: QC.Overlay.overlay
        modal: true
        implicitWidth: 460
        title: "Clone Repository"
        closePolicy: QC.Popup.NoAutoClose

        onRejected: {
            if (cloneAreaId.isCloning) {
                Qt.callLater(dialogId.open)
            }
        }

        onClosed: {
            _gitHub.cancelDeviceLoginFlow()
        }

        QQ.Connections {
            target: dialogId._gitHub
            function onAuthorizationSucceeded() {
                dialogId._addingAccount = false
            }
        }

        contentItem: ColumnLayout {
            spacing: 8

            QC.Label {
                objectName: "deepLinkRepoLabel"
                Layout.fillWidth: true
                text: dialogId._repoDisplay
                font.bold: true
                wrapMode: QC.Label.WordWrap
            }

            RowLayout {
                objectName: "deepLinkAccountRow"
                Layout.fillWidth: true
                visible: !dialogId._isSshUrl

                QC.Label {
                    text: qsTr("Account:")
                    color: Theme.textSubtle
                }

                QC.ComboBox {
                    id: accountCombo
                    objectName: "deepLinkAccountCombo"
                    Layout.fillWidth: true
                    textRole: "label"
                    enabled: !cloneAreaId.isCloning

                    model: RemoteAccountSelectionModel {
                        id: accountSelectionModel
                        sourceModel: RootData.remote.accountModel

                        onEntriesChanged: {
                            // Only auto-select when no account has been chosen yet
                            if (dialogId._selectedUsername !== "") return
                            dialogId._autoSelectAccount()
                        }
                    }

                    delegate: QQ.Loader {
                        required property int entryType
                        required property string label
                        required property string username
                        required property int index

                        width: accountCombo.width
                        sourceComponent: entryType === RemoteAccountSelectionModel.SeparatorEntry
                            ? separatorDelegate : accountItemDelegate
                    }

                    QQ.Component {
                        id: accountItemDelegate
                        QC.ItemDelegate {
                            text: parent.label
                            onClicked: {
                                accountCombo.currentIndex = parent.index
                                if (parent.entryType === RemoteAccountSelectionModel.AccountEntry) {
                                    dialogId._selectedUsername = parent.username
                                    dialogId._addingAccount = false
                                    dialogId._activateSelectedAccount()
                                } else if (parent.entryType === RemoteAccountSelectionModel.AddEntry) {
                                    dialogId._selectedUsername = ""
                                    dialogId._addingAccount = true
                                    RootData.remote.gitHubIntegration.active = true
                                    RootData.remote.accountCoordinator.startAddGitHubAccount()
                                } else {
                                    dialogId._selectedUsername = ""
                                    dialogId._addingAccount = false
                                }
                                accountCombo.popup.close()
                            }
                            highlighted: hovered
                        }
                    }

                    QQ.Component {
                        id: separatorDelegate
                        QQ.Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: 5
                            height: 1
                            color: Theme.divider
                        }
                    }
                }
            }

            // Inline GitHub auth flow — shown when adding an account or re-authenticating
            QC.GroupBox {
                Layout.fillWidth: true
                title: qsTr("GitHub")
                visible: dialogId._showAuthFlow

                GitHubAuthFlow {
                    objectName: "deepLinkGitHubAuthFlow"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    gitHubIntegration: dialogId._gitHub
                    contextMessage: qsTr("Sign in to GitHub to clone this repository.")
                }
            }

            CloneArea {
                id: cloneAreaId
                Layout.fillWidth: true
                urlText: dialogId.repoUrl
                authErrorMessage: qsTr("Sign in to GitHub above to retry.")
                onReadyToOpen: function(filePath) { rootId.openRequested(filePath) }
            }
        }

        footer: QQ.Item {
            implicitHeight: footerRowId.implicitHeight + 16

            RowLayout {
                id: footerRowId
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    leftMargin: 8
                    rightMargin: 8
                }
                spacing: 8

                QC.Button {
                    objectName: "deepLinkCancelButton"
                    text: "Cancel"
                    enabled: !cloneAreaId.isCloning
                    onClicked: dialogId.reject()
                }

                QQ.Item { Layout.fillWidth: true }

                QC.Button {
                    objectName: "deepLinkCloneButton"
                    text: cloneAreaId.isCloning ? "Cloning..." : "Clone && Open"
                    enabled: !cloneAreaId.isCloning
                    font.bold: true
                    onClicked: {
                        if (!dialogId._isSshUrl) {
                            dialogId._activateSelectedAccount()
                        }
                        cloneAreaId.clone()
                    }
                }
            }
        }
    }
}
