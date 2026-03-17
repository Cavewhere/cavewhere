
import QtQuick.Dialogs
import cavewherelib

FileDialog {
    id: saveAsFileDialogId
    readonly property bool bundledFirst: {
        const fileType = RootData.project.fileType;
        return fileType === Project.BundledGitFileType
                || fileType === Project.SqliteFileType;
    }
    function finalProjectPathForSelection(localPath, extension) {
        const lower = localPath.toLowerCase();
        let normalizedPath = localPath;
        if (lower.endsWith(".cwproj")) {
            normalizedPath = localPath.slice(0, localPath.length - ".cwproj".length);
        } else if (lower.endsWith(".cw")) {
            normalizedPath = localPath.slice(0, localPath.length - ".cw".length);
        }

        if (extension === ".cw") {
            return normalizedPath + extension;
        }

        const separatorNeeded = normalizedPath.endsWith("/") || normalizedPath.endsWith("\\") ? "" : "/"
        const slashIndex = Math.max(normalizedPath.lastIndexOf("/"), normalizedPath.lastIndexOf("\\"))
        const baseName = slashIndex >= 0 ? normalizedPath.slice(slashIndex + 1) : normalizedPath
        return normalizedPath + separatorNeeded + baseName + extension
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
        const extension = selectedNameFilter
                && selectedNameFilter.index !== undefined
                && selectedNameFilter.index >= 0
                && selectedNameFilter.index < nameFilters.length
                && nameFilters[selectedNameFilter.index].toString().indexOf("*.cwproj") !== -1
                ? ".cwproj"
                : ".cw";
        const saveTargetPath = finalProjectPathForSelection(localPath, extension)

        if (RootData.project.saveAs(saveTargetPath)) {
            RootData.lastDirectory = selectedFile;
        }
    }
}
