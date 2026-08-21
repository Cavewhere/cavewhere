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

// "Your external sources have moved on" — the project-scope strip of
// plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §4, shown at the top of the
// window and never as a dialog.
//
// A dumb presenter: it counts nothing and copies nothing. The host binds
// the count and answers the three signals, which is what keeps the same
// strip usable wherever a count comes from.
QQ.Rectangle {
    id: root
    objectName: "externalSourceChangeBanner"

    // How many attachments read Changed right now.
    required property int changedCount

    signal updateAllRequested()
    signal showRequested()
    signal dismissRequested()

    implicitHeight: bannerRowId.implicitHeight + Theme.sectionSpacing * 2
    implicitWidth: bannerRowId.implicitWidth + Theme.sectionSpacing * 2
    color: Theme.info
    radius: Theme.floatingWidgetRadius
    border.width: 1
    border.color: Theme.border

    RowLayout {
        id: bannerRowId

        anchors.fill: parent
        anchors.margins: Theme.sectionSpacing
        spacing: Theme.flowSpacing

        QC.Label {
            objectName: "externalSourceChangeMessage"

            Layout.fillWidth: true
            wrapMode: QC.Label.WordWrap
            font.pixelSize: Theme.fontSizeBody
            text: root.changedCount === 1
                  ? qsTr("1 external source has changed since it was copied into this project.")
                  : qsTr("%1 external sources have changed since they were copied into this project.")
                    .arg(root.changedCount)
        }

        QC.Button {
            objectName: "externalSourceUpdateAllButton"

            text: qsTr("Update all")
            onClicked: root.updateAllRequested()
        }

        QC.Button {
            objectName: "externalSourceShowButton"

            text: qsTr("Show")
            onClicked: root.showRequested()
        }

        QC.Button {
            objectName: "externalSourceDismissButton"

            text: "✕"
            QC.ToolTip.text: qsTr("Hide this until a source changes again")
            QC.ToolTip.delay: Theme.toolTipDelay
            QC.ToolTip.visible: hovered
            onClicked: root.dismissRequested()
        }
    }
}
