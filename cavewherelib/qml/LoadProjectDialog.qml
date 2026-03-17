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

        let fileType = RootData.project.projectType(path);

        switch(fileType) {
        case Project.UnknownFileType:
            //Some how add an error
            console.log("Unknown file type")
            break;
        case Project.SqliteFileType:
            RootData.project.loadFile(path);
            break;

        case Project.GitFileType:
            RootData.project.loadFile(path);
            break;
        case Project.BundledGitFileType:
            RootData.project.loadFile(path);
            break;
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
