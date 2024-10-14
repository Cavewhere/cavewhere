
import QtQuick.Dialogs
import cavewherelib

FileDialog {
    id: saveAsFileDialogId
    nameFilters: ["CaveWhere Project (*.cw)"]
    title: "Save CaveWhere Project As"
    fileMode: FileDialog.SaveFile
    currentFolder: RootData.lastDirectory
    onAccepted: {
        RootData.lastDirectory = selectedFile
        RootData.project.saveAs(selectedFile)
    }
}
