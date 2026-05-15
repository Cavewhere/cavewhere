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

    property url selectedDestinationFolder: ""

    readonly property bool isCloning: cloneWatcherId.state === GitFutureWatcher.Loading
    readonly property int cloneErrorKind: clonerId.cloneErrorKind

    // Emitted after clone succeeds with the path of the cloned project file.
    // The caller is responsible for asking to save the current project and then loading the file.
    signal readyToOpen(string filePath)

    function clone() {
        clonerId.clone(urlText, _destinationParentFolder)
    }

    function reset() {
        clonerId.resetCloneState()
    }

    function isSshUrl(url) {
        return clonerId.isSshUrl(url)
    }

    readonly property url _destinationParentFolder: {
        if (root.selectedDestinationFolder.toString() !== "")
            return root.selectedDestinationFolder
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

        onAccepted: {
            root.selectedDestinationFolder = folderDialogId.selectedFolder
        }
    }

    RowLayout {
        Layout.fillWidth: true

        QC.Label {
            text: "Destination:"
            color: Theme.textSubtle
        }

        LinkText {
            Layout.fillWidth: true
            elide: QC.Label.ElideLeft
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
            wrapMode: QC.Label.WrapAtWordBoundaryOrAnywhere
            color: Theme.textSecondary
        }

        ErrorHelpArea {
            objectName: "remoteCloneErrorArea"
            Layout.fillWidth: true
            text: clonerId.cloneErrorFriendlyMessage.length > 0
                  ? clonerId.cloneErrorFriendlyMessage
                  : clonerId.cloneErrorMessage
            visible: clonerId.cloneErrorKind !== RemoteRepositoryCloner.None

            onLinkActivated: function(link) { Qt.openUrlExternally(link) }
        }

        QC.Label {
            objectName: "remoteCloneErrorDetails"
            Layout.fillWidth: true
            visible: clonerId.cloneErrorFriendlyMessage.length > 0
                     && clonerId.cloneErrorMessage.length > 0
            text: qsTr("Details: %1").arg(clonerId.cloneErrorMessage)
            color: Theme.textSubtle
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: QC.Label.WrapAtWordBoundaryOrAnywhere
        }
    }
}
