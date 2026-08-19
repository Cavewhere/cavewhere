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

// Attached-mode header for the external-centerline trip panel:
// entry filename + format + the file actions (Reload now / Replace…),
// and the remembered-source line ("Source: <path>" or "Source
// forgotten (this machine)"). The actions only raise signals — the
// panel owns the manager wiring.
ColumnLayout {
    id: root
    objectName: "attachedHeader"

    property Trip trip: null
    property ExternalSourceSettings externalSourceSettings: null
    property bool actionsEnabled: true

    // Refreshed imperatively: breadcrumbsChanged is a plain signal, so
    // a declarative binding can't observe it.
    property string rememberedSourcePath: ""

    readonly property string entryFile: trip !== null ? trip.externalCenterline.entryFile : ""
    readonly property string fileName: entryFile.substring(entryFile.lastIndexOf("/") + 1)
    readonly property string formatName: trip !== null ? trip.externalCenterline.format : ""
    readonly property bool attached: entryFile.length > 0

    spacing: Theme.tightSpacing

    signal reloadRequested()
    signal replaceRequested()

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
        }

        QC.Label {
            objectName: "attachedFormatLabel"
            visible: root.formatName.length > 0
            color: Theme.textSecondary
            text: root.formatName
        }

        QQ.Item { Layout.fillWidth: true }

        QC.Button {
            objectName: "reloadButton"
            text: qsTr("Reload now")
            visible: root.attached
            enabled: root.actionsEnabled
            onClicked: root.reloadRequested()
        }

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
              ? qsTr("Source: %1").arg(root.rememberedSourcePath)
              : qsTr("Source forgotten (this machine)")
    }
}
