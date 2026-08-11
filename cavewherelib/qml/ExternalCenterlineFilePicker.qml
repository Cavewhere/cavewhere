/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Dialogs as QD
import QtQuick.Layouts
import cavewherelib

// Survey-file picker shared by the dialogs that take an entry file: a
// path field with Browse over a live scan preview, and the preview's
// verdict - detected format, file count, warnings, errors - underneath.
//
// The host feeds its own operation's failure in through
// operationError, which outranks the scan's own message. Editing the
// path signals pathEdited so the host can drop that failure: a stale
// error must not outrank the fresh path's scan result.
ColumnLayout {
    id: root

    property string promptText: ""
    property string fileDialogTitle: ""
    property bool locked: false
    property string operationError: ""

    // Folder the Browse dialog opens in. Empty - which is what a host
    // with no breadcrumb to offer passes - falls back to the last
    // directory the user picked anything in.
    property url initialFolder

    // The add dialog opens on an empty field and names what it accepts.
    // A replace picks the same kinds of file for a trip that already has
    // one, so it stays quiet.
    property bool showsSupportedFormats: true

    readonly property alias sourcePath: previewId.sourcePath
    readonly property alias valid: previewId.valid
    readonly property alias importSupported: previewId.importSupported

    signal pathEdited()

    function clear() {
        pathFieldId.text = ""
    }

    spacing: Theme.tightSpacing

    ExternalCenterlineScanPreview {
        id: previewId
        sourcePath: pathFieldId.text
    }

    QC.Label {
        visible: root.promptText.length > 0
        text: root.promptText
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.tightSpacing

        QC.TextField {
            id: pathFieldId
            objectName: "sourcePathField"
            Layout.fillWidth: true
            enabled: !root.locked
            placeholderText: qsTr("/path/to/centerline.svx")
            onTextChanged: root.pathEdited()
        }

        QC.Button {
            objectName: "browseButton"
            enabled: !root.locked
            text: qsTr("Browse…")
            onClicked: fileDialogId.open()
        }
    }

    QC.Label {
        objectName: "supportedFormatsLabel"
        Layout.fillWidth: true
        visible: root.showsSupportedFormats && previewId.formatName.length === 0
        wrapMode: QC.Label.WordWrap
        color: Theme.textSubtle
        font.pixelSize: Theme.fontSizeSmall
        text: qsTr("You can open Survex (.svx), Compass (.dat, .mak), "
                 + "or Walls (.wpj, .srv) files.")
    }

    QC.Label {
        objectName: "formatLabel"
        visible: previewId.formatName.length > 0
        color: Theme.textSecondary
        text: qsTr("Format: %1 (auto-detected)").arg(previewId.formatName)
    }

    QC.Label {
        objectName: "scanningLabel"
        visible: previewId.scanning
        color: Theme.textSecondary
        text: qsTr("Scanning…")
    }

    QC.Label {
        objectName: "scanSummaryLabel"
        visible: previewId.valid
        text: qsTr("✓ Found %n file(s)", "", previewId.fileCount)
    }

    QC.Label {
        objectName: "scanWarningLabel"
        Layout.fillWidth: true
        visible: previewId.warnings.length > 0
        wrapMode: QC.Label.WordWrap
        color: Theme.warning
        text: previewId.warnings.join("\n")
    }

    QC.Label {
        objectName: "scanErrorLabel"
        Layout.fillWidth: true
        visible: text.length > 0
        wrapMode: QC.Label.WordWrap
        color: Theme.danger
        text: root.operationError.length > 0 ? root.operationError
                                             : previewId.errorMessage
    }

    QD.FileDialog {
        id: fileDialogId
        objectName: "entryFileDialog"
        title: root.fileDialogTitle
        nameFilters: [
            qsTr("Survey files (*.svx *.dat *.mak *.srv *.wpj)"),
            qsTr("All files (*)")
        ]
        currentFolder: root.initialFolder.toString().length > 0
                       ? root.initialFolder
                       : RootData.lastDirectory
        fileMode: QD.FileDialog.OpenFile
        onAccepted: {
            RootData.lastDirectory = selectedFile
            pathFieldId.text = RootData.urlToLocal(selectedFile)
        }
    }
}
