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

// The banner's [Show] list (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §4):
// one compact row per attachment whose source needs attention — who owns
// it, where it was copied from, how it stands — with the per-row Update
// for the user who diverged one file on purpose.
//
// Dumb like the banner: rows in, updateRequested out. The host builds the
// rows and runs the verb.
QQ.Rectangle {
    id: root
    objectName: "externalSourceChangeList"

    // One element per row, with the roles the delegate reads below.
    required property QQ.ListModel rows

    // ownerKey of the row whose Update was pressed — the string spelling of
    // the owner id, which is what JavaScript can carry.
    signal updateRequested(string ownerKey)

    // The list scrolls past Theme's cap so a project with many attachments
    // still leaves the window usable.
    implicitHeight: Math.min(Theme.floatingListMaxHeight,
                             rowsViewId.contentHeight + Theme.sectionSpacing * 2)
    color: Theme.surface
    radius: Theme.floatingWidgetRadius
    border.width: 1
    border.color: Theme.border

    QC.ScrollView {
        id: listScrollId

        anchors.fill: parent
        anchors.margins: Theme.sectionSpacing
        clip: true

        QQ.ListView {
            id: rowsViewId
            objectName: "externalSourceChangeRows"

            model: root.rows
            spacing: Theme.tightSpacing
            boundsBehavior: QQ.Flickable.StopAtBounds

            delegate: RowLayout {
                id: rowId
                objectName: "externalSourceChangeRow"

                required property string ownerKey
                required property string ownerName
                required property string ownerKind
                required property string caveName
                required property string sourcePath
                required property int status

                width: rowsViewId.width
                spacing: Theme.flowSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QC.Label {
                        objectName: "externalSourceChangeRowOwner"

                        Layout.fillWidth: true
                        elide: QC.Label.ElideRight
                        font.pixelSize: Theme.fontSizeBody
                        // The cave names the trip: the same trip name lives
                        // in more than one cave.
                        text: rowId.ownerKind === "Trip"
                              ? qsTr("%1 — %2").arg(rowId.caveName).arg(rowId.ownerName)
                              : rowId.ownerName
                    }

                    QC.Label {
                        objectName: "externalSourceChangeRowSource"

                        Layout.fillWidth: true
                        elide: QC.Label.ElideLeft
                        color: Theme.textSubtle
                        font.pixelSize: Theme.fontSizeSmall
                        text: rowId.sourcePath
                    }
                }

                QC.Label {
                    objectName: "externalSourceChangeRowStatus"

                    font.pixelSize: Theme.fontSizeSmall
                    text: rowId.status === ExternalSourceStatusModel.Changed
                          ? qsTr("Changed")
                          : qsTr("Source missing")
                }

                QC.Button {
                    objectName: "externalSourceChangeRowUpdateButton"

                    text: qsTr("Update")
                    // A gone source has nothing to copy from, so the row
                    // says so and offers nothing.
                    enabled: rowId.status === ExternalSourceStatusModel.Changed
                    onClicked: root.updateRequested(rowId.ownerKey)
                }
            }
        }
    }
}
