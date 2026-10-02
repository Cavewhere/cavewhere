/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.ListView {
    id: taskListView

    // Every tracked job by default; a caller (or a test) can substitute its own.
    model: ActiveTasks.model

    delegate: QQ.Rectangle {
        id: delegateId

        required property string nameRole
        required property int progressRole
        required property int numberOfStepsRole
        required property string detailNameRole
        required property real detailProgressRole
        required property real detailTotalRole
        required property bool treeBackedRole

        readonly property bool hasDetail: detailNameRole !== ""

        anchors.left: parent ? parent.left : undefined
        anchors.right: parent ? parent.right : undefined
        height: columnLayoutId.implicitHeight + Theme.delegatePadding * 2

        color: Theme.background

        // Group digits for readability (e.g. 1,234,567). The app runs under the
        // C locale (no digit grouping), so toLocaleString is a no-op here —
        // insert the thousands separators manually instead.
        function formatCount(value) {
            return String(value).replace(/\B(?=(\d{3})+(?!\d))/g, ",");
        }

        ColumnLayout {
            id: columnLayoutId

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Theme.delegatePadding

            spacing: Theme.tightSpacing

            QC.Label {
                id: nameText

                text: delegateId.nameRole
                Layout.fillWidth: true
                wrapMode: QQ.Text.WordWrap
            }

            QC.ProgressBar {
                id: progressBar

                Layout.fillWidth: true
                indeterminate: delegateId.numberOfStepsRole <= 0
                value: indeterminate ? 0.0 : delegateId.progressRole / delegateId.numberOfStepsRole
            }

            QC.Label {
                id: progressDetailText

                objectName: "taskProgressCaption"

                Layout.fillWidth: true
                // A tree-backed row counts in promise units, which mean nothing
                // to a person, so it reads as a percent. Real counts belong to
                // the detail line below.
                text: delegateId.numberOfStepsRole > 0
                      ? (delegateId.treeBackedRole
                         ? qsTr("%1%").arg(Math.round(100 * delegateId.progressRole / delegateId.numberOfStepsRole))
                         : qsTr("%1 / %2")
                            .arg(delegateId.formatCount(delegateId.progressRole))
                            .arg(delegateId.formatCount(delegateId.numberOfStepsRole)))
                      : qsTr("working…")
                color: Theme.textSubtle
                font.pixelSize: Theme.fontSizeCaption
            }

            QC.ProgressBar {
                id: detailProgressBar

                objectName: "taskDetailProgressBar"

                visible: delegateId.hasDetail
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.taskDetailBarHeight
                // An opaque leaf reports no total, so the bar pulses to show the
                // work is alive while the row's own bar holds its last value.
                indeterminate: delegateId.detailTotalRole <= 0
                value: indeterminate ? 0.0 : delegateId.detailProgressRole / delegateId.detailTotalRole
            }

            QC.Label {
                id: detailNameText

                objectName: "taskDetailLabel"

                visible: delegateId.hasDetail
                Layout.fillWidth: true
                text: delegateId.detailTotalRole > 0
                      ? qsTr("%1 %2 / %3")
                            .arg(delegateId.detailNameRole)
                            .arg(delegateId.formatCount(delegateId.detailProgressRole))
                            .arg(delegateId.formatCount(delegateId.detailTotalRole))
                      : delegateId.detailNameRole
                color: Theme.textSubtle
                font.pixelSize: Theme.fontSizeCaption
                elide: QQ.Text.ElideMiddle
            }
        }
    }
}
