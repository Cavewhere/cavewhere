
import QtQuick.Dialogs
import cavewherelib

FileDialog {
    id: saveAsFileDialogId
    nameFilters: [
        "CaveWhere Bundled Project (*.cw)",
        "CaveWhere Project Directory (*.cwproj)"
    ]
    title: "Save CaveWhere Project As"
    fileMode: FileDialog.SaveFile
    currentFolder: RootData.lastDirectory

    function selectedExtension()
    {
        const filter = selectedNameFilter ? selectedNameFilter.toString() : "";
        if (filter.indexOf("*.cwproj") !== -1) {
            return ".cwproj";
        }
        return ".cw";
    }

    function ensureExtension(localPath, ext)
    {
        const lower = localPath.toLowerCase();
        if (lower.endsWith(".cw") || lower.endsWith(".cwproj")) {
            return localPath;
        }
        return localPath + ext;
    }

    function extensionForFilter(filterText)
    {
        const filter = filterText ? filterText.toString() : "";
        if (filter.indexOf("*.cwproj") !== -1) {
            return ".cwproj";
        }
        return ".cw";
    }

    function runSaveAsForTest(localPath, filterText)
    {
        const targetPath = ensureExtension(localPath, extensionForFilter(filterText));
        if (RootData.project.saveAs(targetPath)) {
            RootData.lastDirectory = selectedFile;
            return targetPath;
        }
        return "";
    }

    onAccepted: {
        const localPath = RootData.urlToLocal(selectedFile);
        runSaveAsForTest(localPath, selectedNameFilter);
    }
}
