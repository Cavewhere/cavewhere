import QtQuick
import QtQuick.Dialogs
import cavewherelib

Item {
    anchors.fill: parent

    property alias loadFileDialog: loadFileDialogId

    FileDialog {
        id: loadFileDialogId

        property string oldFilePath;

        nameFilters: ["CaveWhere File (*.cw)"]
        currentFolder: RootData.lastDirectory
        onAccepted: {
            RootData.lastDirectory = selectedFile
            RootData.pageSelectionModel.clearHistory();
            RootData.pageSelectionModel.gotoPageByName(null, "View")

            let fileType = RootData.project.projectType(selectedFile);
            console.log("Filetype:" + fileType)

            switch(fileType) {
            case Project.UnknownFileType:
                //Some how add an error
                console.log("Unknown file type")
                break;
            case Project.SqliteFileType:
                console.log("Sqlite file:" + Project.SqliteFileType)
                oldFilePath = RootData.urlToLocal(selectedFile);
                convertedDialog.filename = RootData.fileName(oldFilePath)
                convertedDialog.repositoryName = RootData.fileBaseName(oldFilePath);
                convertedDialog.open();
                break;


            case Project.GitFileType:
                RootData.project.loadFile(selectedFile);
                break;
            }

            // RootData.project.loadFile(selectedFile)
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
