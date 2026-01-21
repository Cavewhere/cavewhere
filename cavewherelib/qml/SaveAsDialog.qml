
import QtQuick.Dialogs
import cavewherelib

FileDialog {
    id: saveAsFileDialogId
    nameFilters: ["CaveWhere Project (*.cwproj)"]
    title: "Save CaveWhere Project As"
    fileMode: FileDialog.SaveFile
    currentFolder: RootData.lastDirectory
    onAccepted: {
        const chosenFile = selectedFile;
        if (RootData.project.saveAs(chosenFile)) {
            RootData.lastDirectory = chosenFile;
        }
    }
}
