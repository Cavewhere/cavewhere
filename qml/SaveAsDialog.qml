import QtQuick 2.0
import QtQuick.Dialogs 1.2

FileDialog {
    id: saveAsFileDialogId
    nameFilters: ["Cavewhere Project (*.cw)"]
    title: "Save Cavewhere Project As"
    selectExisting: false
    folder: rootData.lastDirectory
    onAccepted: {
        rootData.lastDirectory = fileUrl
        project.saveAs(fileUrl)
    }
}
