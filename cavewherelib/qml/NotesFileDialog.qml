import QtQuick.Dialogs
import cavewherelib

FileDialog {
    id: notesFileDialog

    signal filesSelected(list<url> images)

    nameFilters: ["All (" + RootData.supportImageFormats + " *.glb)"]
    title: "Load Images or LiDAR scans"
    fileMode: FileDialog.OpenFiles
    currentFolder: RootData.lastDirectory
    onAccepted: {
        RootData.lastDirectory = notesFileDialog.currentFolder
        notesFileDialog.filesSelected(notesFileDialog.selectedFiles)
    }
}
