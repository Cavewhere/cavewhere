pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Dialogs
import cavewherelib
import QQuickGit

ColumnLayout {
    id: root

    // URL to clone — set by the parent (text field or deep link)
    property string urlText

    // Auth-error message — callers can override for their context
    property string authErrorMessage: qsTr("Select a GitHub account to clone from GitHub.")

    readonly property bool isCloning: cloneWatcherId.state === GitFutureWatcher.Loading
    readonly property bool cloneFailedDueToAuthError: clonerId.cloneFailedDueToAuthError

    // Emitted after clone succeeds with the path of the cloned project file.
    // The caller is responsible for asking to save the current project and then loading the file.
    signal readyToOpen(string filePath)

    function clone() {
        clonerId.clone(urlText, _destinationParentFolder)
    }

    readonly property url _destinationParentFolder: {
        if (folderDialogId.selectedFolder.toString() !== "")
            return folderDialogId.selectedFolder
        if (folderDialogId.currentFolder.toString() !== "")
            return folderDialogId.currentFolder
        return RootData.recentProjectModel.defaultRepositoryDir
    }

    readonly property string destinationPathText: {
        const repoName = clonerId.repositoryNameFromUrl(urlText)
        const parentPath = RootData.urlToLocal(_destinationParentFolder)
        if (parentPath.length === 0) return repoName
        if (repoName.length === 0) return parentPath
        const sep = parentPath.endsWith("/") || parentPath.endsWith("\\") ? "" : "/"
        return parentPath + sep + repoName
    }

    RemoteRepositoryCloner {
        id: clonerId
        recentProjectModel: RootData.recentProjectModel
        cloneWatcher: cloneWatcherId
        account: RootData.account
        gitHubIntegration: RootData.remote.gitHubIntegration

        onRepositoryClonedIndex: function(clonedIndex) {
            if (clonedIndex < 0) {
                console.warn("CloneArea: Cloned repository not found in model.")
                return
            }
            const fileResult = RootData.recentProjectModel.repositoryProjectFile(clonedIndex)
            if (fileResult.hasError) {
                console.warn("CloneArea: Failed to open cloned repository:", fileResult.errorMessage)
                return
            }
            root.readyToOpen(fileResult.value)
        }

        onRepositoryClonedWithRemote: function(repositoryPath, remoteUrl) {
            RootData.remote.accountCoordinator.bindRemoteToActiveGitHubAccount(remoteUrl)
        }
    }

    GitFutureWatcher {
        id: cloneWatcherId
        initialProgressText: "Cloning"
    }

    FolderDialog {
        id: folderDialogId
        currentFolder: RootData.recentProjectModel.defaultRepositoryDir
        selectedFolder: currentFolder
    }

    RowLayout {
        Layout.fillWidth: true

        QC.Label {
            text: "Destination:"
            color: Theme.textSubtle
        }

        LinkText {
            Layout.fillWidth: true
            elide: QQ.Text.ElideLeft
            text: root.destinationPathText
            onClicked: folderDialogId.open()
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        visible: cloneWatcherId.state === GitFutureWatcher.Loading
                 || clonerId.cloneErrorMessage.length > 0
                 || clonerId.cloneStatusMessage.length > 0

        QC.ProgressBar {
            Layout.fillWidth: true
            visible: cloneWatcherId.state === GitFutureWatcher.Loading
            from: 0
            to: 1
            value: cloneWatcherId.progress
        }

        QC.Label {
            objectName: "remoteCloneStatusText"
            Layout.fillWidth: true
            visible: clonerId.cloneStatusMessage.length > 0
            text: clonerId.cloneStatusMessage
            wrapMode: QQ.Text.WrapAtWordBoundaryOrAnywhere
            color: Theme.textSecondary
        }

        ErrorHelpArea {
            objectName: "remoteCloneErrorArea"
            Layout.fillWidth: true
            text: clonerId.cloneFailedDueToAuthError ? root.authErrorMessage : clonerId.cloneErrorMessage
            visible: clonerId.cloneErrorMessage.length > 0
        }
    }
}
