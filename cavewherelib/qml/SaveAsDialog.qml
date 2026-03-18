pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Dialogs
import cavewherelib

// Custom Save As dialog replacing the old single-native-file-dialog approach.
//
// For temporary (first-save) projects it shows:
//   - An editable project name field (pre-filled from region.name).
//   - A format picker: Directory (.cwproj) / Bundle (.cw).
//   - A destination path row with a native Browse button.
//
// For already-saved projects it shows only:
//   - The format picker.
//   - The destination path row (project name is locked to the current region name).
//
// The path derivation logic from finalProjectPathForSelection() is preserved
// so that existing test helpers can still use it directly.

QC.Dialog {
    id: rootId

    title: "Save CaveWhere Project As"
    modal: true
    implicitWidth: 540
    anchors.centerIn: parent
    standardButtons: QC.DialogButtonBox.Save | QC.DialogButtonBox.Cancel

    // ── Public state ─────────────────────────────────────────────────────────

    // True when Bundle (.cw) is the selected output format.
    property bool bundleFormat: false

    // Parent folder for directory saves, or the parent directory of the bundle file.
    // Initialized from RootData.lastDirectory when the dialog opens.
    property string destinationFolder: ""

    // When non-empty the user has explicitly chosen a full bundle file path via
    // the native FileDialog; used verbatim and takes precedence over the derived path.
    property string bundleFilePath: ""

    // Editable name for the project, buffered locally so that typing in the name
    // field does NOT mutate RootData.region.name until the user presses Save.
    // Initialized from RootData.region.name when the dialog opens.
    property string _pendingName: ""

    // The full save path that will be passed to Project.saveAs(). Updates
    // reactively whenever bundleFormat, destinationFolder, bundleFilePath, or
    // _pendingName changes.
    readonly property string resolvedSavePath: {
        if (bundleFormat) {
            return bundleFilePath.length > 0 ? bundleFilePath : _projectFolder + ".cw"
        } else {
            return _projectFolder + "/" + _sanitizedName + ".cwproj"
        }
    }

    // ── Conflict detection ────────────────────────────────────────────────────

    // Sanitized pending name — cached so all downstream bindings share one evaluation.
    readonly property string _sanitizedName: RootData.sanitizeFileName(_pendingName)

    // The project folder that would be created in directory mode.
    readonly property string _projectFolder: destinationFolder + "/" + _sanitizedName

    // True when a directory-format save would collide with an existing folder.
    // Blocks the Save button.
    readonly property bool _directoryConflict: {
        return !bundleFormat && RootData.pathExists(_projectFolder)
    }

    // True when a bundle-format save would overwrite an existing file.
    // Shows a warning but does not block Save.
    readonly property bool _bundleConflict: {
        return bundleFormat && RootData.pathExists(resolvedSavePath)
    }

    // True when in directory mode and the parent folder the user typed does not exist.
    // Blocks the Save button.
    readonly property bool _parentNotFound: {
        return !bundleFormat && !RootData.pathExists(destinationFolder)
    }

    // Bind the Save button enabled state reactively after the button is created.
    QQ.Binding {
        target: rootId.standardButton(QC.DialogButtonBox.Save)
        property: "enabled"
        value: !rootId._directoryConflict && !rootId._parentNotFound
    }

    // ── Helpers ──────────────────────────────────────────────────────────────

    // Strips any number of trailing .cw / .cwproj extensions then appends .cw.
    // Needed because macOS SaveFile dialogs can double-append extensions
    // (e.g. the user types "test" with a stale .cwproj name in the field,
    // yielding "test.cwproj.cw" from the native picker).
    function _normalizeBundleFilePath(rawPath) {
        let base = rawPath
        for (;;) {
            const lower = base.toLowerCase()
            if (lower.endsWith(".cwproj")) {
                base = base.slice(0, -7)
            } else if (lower.endsWith(".cw")) {
                base = base.slice(0, -3)
            } else {
                break
            }
        }
        return base + ".cw"
    }

    // Preserved for backward-compatibility with existing tests that verify path
    // derivation by calling finalProjectPathForSelection() directly.
    function finalProjectPathForSelection(localPath, extension) {
        const lower = localPath.toLowerCase()
        let p = localPath
        if (lower.endsWith(".cwproj")) {
            p = localPath.slice(0, -7)
        } else if (lower.endsWith(".cw")) {
            p = localPath.slice(0, -3)
        }
        if (extension === ".cw") {
            return p + extension
        }
        const sep = (p.endsWith("/") || p.endsWith("\\")) ? "" : "/"
        const idx = Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\"))
        const base = idx >= 0 ? p.slice(idx + 1) : p
        return p + sep + base + extension
    }

    // ── Lifecycle ────────────────────────────────────────────────────────────

    onAboutToShow: {
        const ft = RootData.project.fileType
        bundleFormat = ft === Project.BundledGitFileType || ft === Project.SqliteFileType
        destinationFolder = RootData.urlToLocal(RootData.lastDirectory)
        bundleFilePath = ""
        _pendingName = RootData.region.name
    }

    onAccepted: {
        if (RootData.project.isTemporaryProject) {
            RootData.region.name = _pendingName
        }
        if (RootData.project.saveAs(resolvedSavePath)) {
            // Pass the saved file so setLastDirectory strips the filename and
            // stores the correct containing directory.
            RootData.lastDirectory = Qt.resolvedUrl("file://" + resolvedSavePath)
        }
    }

    // ── Content ──────────────────────────────────────────────────────────────

    contentItem: QC.ScrollView {
        id: _scrollView
        clip: true
        QC.ScrollBar.vertical.policy: QC.ScrollBar.AsNeeded

        ColumnLayout {
            spacing: 10
            width: _scrollView.width

            // Project name row — shown only for first-save (temporary) projects.
            RowLayout {
                visible: RootData.project.isTemporaryProject
                Layout.fillWidth: true
                spacing: 6

                QC.Label { text: "Project Name:" }

                QC.TextField {
                    Layout.fillWidth: true
                    objectName: "saveAsProjectName"
                    text: rootId._pendingName
                    onTextEdited: {
                        rootId._pendingName = text
                        rootId.bundleFilePath = ""
                    }
                }
            }

            // Format picker.
            RowLayout {
                spacing: 16

                QC.RadioButton {
                    text: "Directory (.cwproj)"
                    checked: !rootId.bundleFormat
                    onToggled: if (checked) { rootId.bundleFormat = false; rootId.bundleFilePath = "" }
                }

                QC.RadioButton {
                    text: "Bundle (.cw)"
                    checked: rootId.bundleFormat
                    onToggled: if (checked) { rootId.bundleFormat = true }
                }

                InformationButton {
                    showItemOnClick: formatHelpId
                }
            }

            HelpArea {
                id: formatHelpId
                Layout.fillWidth: true
                text: "<table border='1' cellpadding='4' cellspacing='0' width='100%'>" +
                      "<tr><th></th><th>Directory (.cwproj)</th><th>Bundle (.cw)</th></tr>" +
                      "<tr><td><b>Best for</b></td>         <td align='center'>Active, multi-trip, or team projects</td><td align='center'>Sharing or archiving</td></tr>" +
                      "<tr><td>Continuous auto-save</td>    <td align='center'>✓</td>            <td align='center'>—</td></tr>" +
                      "<tr><td>Single portable file</td>    <td align='center'>—</td>            <td align='center'>✓</td></tr>" +
                      "<tr><td>Speed</td>                   <td align='center'>Fast</td>          <td align='center'>Slower (compresses on save)</td></tr>" +
                      "<tr><td>Direct file access</td>      <td align='center'>✓</td>            <td align='center'>Requires extracting first</td></tr>" +
                      "<tr><td>Version history</td>         <td align='center'>✓</td>            <td align='center'>✓</td></tr>" +
                      "<tr><td>Remote sync</td>             <td align='center'>✓</td>            <td align='center'>✓</td></tr>" +
                      "</table>" +
                      "<p><b>Version history</b> lets you undo mistakes across sessions and see what changed over time. " +
                      "<b>Remote sync</b> lets you back up your project and collaborate with others via an online hosting service (e.g. GitHub).</p>"
            }

            // Location row: editable field + Browse button.
            // Bundle — one fully-editable field for the complete path.
            // Directory — editable parent folder field only.
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                QC.Label { text: "Location:" }

                // Bundle mode: user edits the full save path directly.
                QC.TextField {
                    Layout.fillWidth: true
                    visible: rootId.bundleFormat
                    objectName: "saveAsPathField"
                    text: rootId.resolvedSavePath
                    onTextEdited: {
                        rootId.bundleFilePath = text
                        rootId.destinationFolder = text.replace(/\/[^/]*$/, "")
                    }
                }

                // Directory mode: editable parent folder.
                QC.TextField {
                    Layout.fillWidth: true
                    visible: !rootId.bundleFormat
                    objectName: "saveAsParentDirField"
                    text: rootId.destinationFolder
                    onTextEdited: rootId.destinationFolder = text
                }

                QC.Button {
                    text: "Browse\u2026"
                    onClicked: {
                        if (rootId.bundleFormat) {
                            _bundlePicker.currentFolder = Qt.resolvedUrl("file://" + rootId.destinationFolder)
                            _bundlePicker.open()
                        } else {
                            _folderPicker.currentFolder = Qt.resolvedUrl("file://" + rootId.destinationFolder)
                            _folderPicker.open()
                        }
                    }
                }
            }

            // Full path preview — directory mode only.
            QC.Label {
                visible: !rootId.bundleFormat
                Layout.fillWidth: true
                text: rootId.resolvedSavePath
                font.pixelSize: 11
                elide: QQ.Text.ElideLeft
                objectName: "saveAsFullPathLabel"
            }

            // Validation message — error (blocks Save) or warning (allows Save).
            QC.Label {
                objectName: "saveAsConflictLabel"
                visible: rootId._directoryConflict || rootId._bundleConflict || rootId._parentNotFound
                Layout.fillWidth: true
                wrapMode: QQ.Text.WordWrap
                color: (rootId._directoryConflict || rootId._parentNotFound) ? Theme.danger : Theme.warning
                text: rootId._directoryConflict
                      ? "A folder \"%1\" already exists in this location. Choose a different name or location.".arg(rootId._sanitizedName)
                      : rootId._parentNotFound
                        ? "The destination folder does not exist. Choose an existing folder or browse to one."
                        : "An existing bundle will be overwritten."
            }
        }
    }

    // ── Inner native pickers ─────────────────────────────────────────────────
    // Declared as direct children of the Dialog so they share the same id scope
    // as contentItem but are non-visual and don't participate in layout.

    FileDialog {
        id: _bundlePicker
        fileMode: FileDialog.SaveFile
        nameFilters: ["Bundle (*.cw)"]
        onAccepted: {
            const local = RootData.urlToLocal(selectedFile)
            rootId.bundleFilePath = rootId._normalizeBundleFilePath(local)
            rootId.destinationFolder = rootId.bundleFilePath.replace(/\/[^/]*$/, "")
        }
    }

    FolderDialog {
        id: _folderPicker
        onAccepted: {
            rootId.destinationFolder = RootData.urlToLocal(selectedFolder)
            rootId.bundleFilePath = ""
        }
    }
}
