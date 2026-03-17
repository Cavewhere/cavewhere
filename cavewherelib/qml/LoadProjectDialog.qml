import QtQuick
import QtQuick.Dialogs
import cavewherelib

Item {
    anchors.fill: parent

    property alias loadFileDialog: loadFileDialogId
    
    function handleSelectedFile(path) {
        RootData.lastDirectory = path
        RootData.pageSelectionModel.clearHistory();
        RootData.pageSelectionModel.gotoPageByName(null, "View")
        RootData.project.loadFile(path);
    }

    Connections {
        target: RootData.project
        function onGitFileOpened(path) {
            const repoResult = RootData.repositoryModel.addRepositoryFromProjectFile(path);
            if (repoResult.hasError) {
                console.warn("Failed to add repository to recent list:", repoResult.errorMessage);
            }
        }
    }

    function runLoadForTest(path) {
        handleSelectedFile(path);
    }

    FileDialog {
        id: loadFileDialogId

        nameFilters: ["CaveWhere Project (*.cwproj *.cw)"]
        currentFolder: RootData.lastDirectory
        onAccepted: {
            handleSelectedFile(selectedFile);
        }
    }
}
