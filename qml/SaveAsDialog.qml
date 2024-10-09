import QtQuick 2.0 as QQ
import QtQuick.Dialogs

FileDialog {
    id: saveAsFileDialogId
    nameFilters: ["CaveWhere Project (*.cw)"]
    title: "Save CaveWhere Project As"
    fileMode: FileDialog.SaveFile
    currentFolder: rootData.lastDirectory
    onAccepted: {
        rootData.lastDirectory = fileUrl
        project.saveAs(fileUrl)
    }
}
