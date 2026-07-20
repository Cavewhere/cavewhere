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
// entry filename + format, and the remembered-source line
// ("Source: <path>" or "Source forgotten (this machine)").
ColumnLayout {
    id: root
    objectName: "attachedHeader"

    property Trip trip: null
    property ExternalSourceSettings externalSourceSettings: null

    // Refreshed imperatively: externalCenterlineSourcesChanged is a plain
    // signal, so a declarative binding can't observe it.
    property string rememberedSourcePath: ""

    readonly property string entryFile: trip !== null ? trip.externalCenterline.entryFile : ""
    readonly property string fileName: entryFile.substring(entryFile.lastIndexOf("/") + 1)
    readonly property string formatName: trip !== null ? trip.externalCenterline.format : ""

    spacing: Theme.tightSpacing

    function updateRememberedSource() {
        if (trip === null || externalSourceSettings === null) {
            rememberedSourcePath = ""
            return
        }
        rememberedSourcePath = externalSourceSettings.sourcePathFor(trip.id)
    }

    onTripChanged: updateRememberedSource()
    onExternalSourceSettingsChanged: updateRememberedSource()
    QQ.Component.onCompleted: updateRememberedSource()

    QQ.Connections {
        target: root.externalSourceSettings
        function onExternalCenterlineSourcesChanged() {
            root.updateRememberedSource()
        }
    }

    RowLayout {
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
