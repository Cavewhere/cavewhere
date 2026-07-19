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

// Yellow missing-source banner for the external-centerline trip panel.
// Hidden by default — the panel binds visible to the manager's
// missing-source state. Re-link is stubbed until Phase 5b's rebind
// dialog; Forget source raises a signal the panel wires to
// cwExternalSourceSettings.clearSourcePath().
QQ.Rectangle {
    id: root
    objectName: "missingSourceBanner"

    property string sourcePath: ""

    readonly property string relinkDisabledReason:
        qsTr("Re-link will be available in a future release; for now use Forget source.")

    visible: false
    color: Theme.warning
    radius: 5
    implicitHeight: contentLayoutId.implicitHeight + Theme.sectionSpacing * 2

    signal relinkRequested()
    signal forgetSourceRequested()

    ColumnLayout {
        id: contentLayoutId
        anchors.fill: parent
        anchors.margins: Theme.sectionSpacing
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "missingSourceLabel"
            Layout.fillWidth: true
            wrapMode: QC.Label.WordWrap
            text: qsTr("Source not found at %1").arg(root.sourcePath)
        }

        QQ.Flow {
            Layout.fillWidth: true
            spacing: Theme.tightSpacing

            QC.Button {
                objectName: "relinkButton"
                text: qsTr("Re-link…")
                enabled: false

                QC.ToolTip.text: root.relinkDisabledReason
                QC.ToolTip.visible: hovered

                onClicked: root.relinkRequested()
            }

            QC.Button {
                objectName: "forgetSourceButton"
                text: qsTr("Forget source")
                onClicked: root.forgetSourceRequested()
            }
        }
    }
}
