
import QtQuick.Dialogs
import cavewherelib

FileDialog {
    id: saveAsFileDialogId
    readonly property bool bundledFirst: {
        const fileType = RootData.project.fileType;
        return fileType === Project.BundledGitFileType
                || fileType === Project.SqliteFileType;
    }

    nameFilters: bundledFirst
                 ? [ "Bundled (*.cw)", "Directory (*.cwproj)" ]
                 : [ "Directory (*.cwproj)", "Bundled (*.cw)" ]
    title: "Save CaveWhere Project As"
    fileMode: FileDialog.SaveFile
    currentFolder: RootData.lastDirectory

    onVisibleChanged: {
        if (!visible) {
            return;
        }
    }

    onAccepted: {
        const localPath = RootData.urlToLocal(selectedFile);
        const lower = localPath.toLowerCase();
        const extension = selectedNameFilter
                && selectedNameFilter.index !== undefined
                && selectedNameFilter.index >= 0
                && selectedNameFilter.index < nameFilters.length
                && nameFilters[selectedNameFilter.index].toString().indexOf("*.cwproj") !== -1
                ? ".cwproj"
                : ".cw";

        let normalizedPath = localPath;
        if (lower.endsWith(".cwproj")) {
            normalizedPath = localPath.slice(0, localPath.length - ".cwproj".length);
        } else if (lower.endsWith(".cw")) {
            normalizedPath = localPath.slice(0, localPath.length - ".cw".length);
        }

        if (RootData.project.saveAs(normalizedPath + extension)) {
            RootData.lastDirectory = selectedFile;
        }
    }
}
