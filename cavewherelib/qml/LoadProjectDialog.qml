import QtQuick
import QtQuick.Dialogs
import cavewherelib

Item {
    anchors.fill: parent

    property alias loadFileDialog: loadFileDialogId

    function handleSelectedFile(path) {
        RootData.lastDirectory = path
        RootData.loadProject(path);
        const repoResult = RootData.recentProjectModel.addRepositoryFromProjectFile(path);
        if (repoResult.hasError) {
            console.warn("Failed to add to recent list:", repoResult.errorMessage);
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
