import QtQuick
import QtQuick.Dialogs
import cavewherelib

Item {
    anchors.fill: parent

    property alias loadFileDialog: loadFileDialogId

    FileDialog {
        id: loadFileDialogId

        property string oldFilePath;

        nameFilters: ["CaveWhere Project (*.cwproj *.cw)"]
        currentFolder: RootData.lastDirectory
        onAccepted: {
            RootData.lastDirectory = selectedFile
            RootData.pageSelectionModel.clearHistory();
            RootData.pageSelectionModel.gotoPageByName(null, "View")

            let fileType = RootData.project.projectType(selectedFile);

            switch(fileType) {
            case Project.UnknownFileType:
                //Some how add an error
                console.log("Unknown file type")
                break;
            case Project.SqliteFileType:
                oldFilePath = RootData.urlToLocal(selectedFile);
                convertedDialog.filename = RootData.fileName(oldFilePath)
                convertedDialog.repositoryName = RootData.fileBaseName(oldFilePath);
                convertedDialog.open();
                break;


            case Project.GitFileType:
                RootData.project.loadFile(selectedFile);
                const repoResult = RootData.repositoryModel.addRepositoryFromProjectFile(selectedFile);
                if (repoResult.hasError) {
                    console.warn("Failed to add repository to recent list:", repoResult.errorMessage);
                }
                break;
            }
        }
    }

    SourceLocationDialog {
        id: convertedDialog

        property string filename;

        description: filename + " is an old CaveWhere file.\nCaveWhere now uses version control and multiple files to save data.\n\nChoose a location to convert and save the data."
        anchors.fill: parent

        onAccepted: (newPath) => {
            console.log("LocalPath:" + loadFileDialogId.oldFilePath + " newPath:" + newPath)
            RootData.project.convertFromProjectV6(loadFileDialogId.oldFilePath, newPath);

        }
    }
}
