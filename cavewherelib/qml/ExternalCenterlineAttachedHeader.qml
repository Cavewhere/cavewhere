/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// Attached-mode header for the external-centerline trip panel: entry
// filename + format + the Replace… action, and the provenance line
// ("Copied from: <path>" or "Copied from an unknown location (this
// machine)"), which names where the copy came from — every action on
// this panel targets the copy inside the project. The action only
// raises a signal — the panel owns the manager wiring.
ColumnLayout {
    id: root
    objectName: "attachedHeader"

    property Trip trip: null
    property ExternalSourceSettings externalSourceSettings: null
    property bool actionsEnabled: true

    // Absolute on-disk path of entryFile, resolved by the panel against
    // the trip's attachment directory. Empty when there is nothing to
    // resolve, which leaves the file-name context menu disabled.
    property string entryFilePath: ""

    // Refreshed each time the file-name context menu opens: the in-project
    // copy can be deleted (the missing-copy case) long after
    // entryFilePath was resolved, and opening a file that is gone would
    // do nothing at all.
    property bool entryFileExists: false

    // Refreshed imperatively: breadcrumbsChanged is a plain signal, so
    // a declarative binding can't observe it.
    property string rememberedSourcePath: ""

    readonly property string entryFile: trip !== null ? trip.externalCenterline.entryFile : ""
    readonly property string fileName: entryFile.substring(entryFile.lastIndexOf("/") + 1)
    readonly property string formatName: trip !== null ? trip.externalCenterline.format : ""
    readonly property bool attached: entryFile.length > 0

    spacing: Theme.tightSpacing

    signal replaceRequested()

    function refreshEntryFileExists() {
        entryFileExists = entryFilePath.length > 0 && RootData.pathExists(entryFilePath)
    }

    function updateRememberedSource() {
        if (trip === null || externalSourceSettings === null) {
            rememberedSourcePath = ""
            return
        }
        rememberedSourcePath = externalSourceSettings.breadcrumbPath(trip.id)
    }

    onTripChanged: updateRememberedSource()
    onExternalSourceSettingsChanged: updateRememberedSource()
    QQ.Component.onCompleted: updateRememberedSource()

    QQ.Connections {
        target: root.externalSourceSettings
        function onBreadcrumbsChanged() {
            root.updateRememberedSource()
        }
    }

    RowLayout {
        objectName: "attachedHeaderFileRow"
        Layout.fillWidth: true
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "attachedFileLabel"
            Layout.maximumWidth: parent.width
            elide: QC.Label.ElideMiddle
            font.bold: true
            text: root.fileName

            // Right-click reaches the file itself — the copy inside the
            // project, which is the file this panel is showing.
            ContextMenuArea {
                id: fileContextMenuId
                objectName: "attachedFileContextMenu"
                anchors.fill: parent

                RevealInFileManagerMenuItem {
                    objectName: "showInFileManagerAction"
                    filePath: root.entryFilePath
                }

                QC.MenuItem {
                    objectName: "openFileAction"
                    text: qsTr("Open")
                    enabled: root.entryFileExists
                    onTriggered: FileRevealer.openFile(root.entryFilePath)
                }
            }

            QQ.Connections {
                target: fileContextMenuId.menu
                function onAboutToShow() {
                    root.refreshEntryFileExists()
                }
            }
        }

        QC.Label {
            objectName: "attachedFormatLabel"
            visible: root.formatName.length > 0
            color: Theme.textSecondary
            text: root.formatName
        }

        QQ.Item { Layout.fillWidth: true }

        QC.Button {
            objectName: "replaceButton"
            text: qsTr("Replace…")
            visible: root.attached
            enabled: root.actionsEnabled
            onClicked: root.replaceRequested()
        }
    }

    QC.Label {
        objectName: "sourceModeLabel"
        Layout.fillWidth: true
        elide: QC.Label.ElideMiddle
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.textSubtle
        text: root.rememberedSourcePath.length > 0
              ? qsTr("Copied from: %1").arg(root.rememberedSourcePath)
              : qsTr("Copied from an unknown location (this machine)")
    }
}
