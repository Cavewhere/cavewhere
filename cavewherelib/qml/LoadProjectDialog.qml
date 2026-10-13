import QtQuick
import QtQuick.Dialogs
import cavewherelib

Item {
    anchors.fill: parent

    property alias loadFileDialog: loadFileDialogId

    function handleSelectedFile(path) {
        RootData.lastDirectory = path
        RootData.loadProject(path);
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
