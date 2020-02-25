import QtQuick 2.0 as QQ
import QtQuick.Dialogs 1.2

FileDialog {
    id: saveAsFileDialogId
    nameFilters: ["CaveWhere Project (*.cw)"]
    title: "Save CaveWhere Project As"
    selectExisting: false
    folder: rootData.lastDirectory
    onAccepted: {
        rootData.lastDirectory = fileUrl
        project.saveAs(fileUrl)
    }
}
