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

    TaskFutureCombineModel {
        id: taskModelCombinerId
        models: [RootData.taskManagerModel, RootData.futureManagerModel]
    }

    model: taskModelCombinerId
    verticalLayoutDirection: QQ.ListView.BottomToTop

    delegate: QQ.Rectangle {
        id: delegateId

        required property string nameRole
        required property int progressRole
        required property int numberOfStepsRole

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
                value: !indeterminate ? delegateId.progressRole / delegateId.numberOfStepsRole : 0.0
            }

            QC.Label {
                id: progressDetailText

                Layout.fillWidth: true
                text: delegateId.numberOfStepsRole > 0
                      ? qsTr("%1 / %2")
                            .arg(delegateId.formatCount(delegateId.progressRole))
                            .arg(delegateId.formatCount(delegateId.numberOfStepsRole))
                      : qsTr("working…")
                color: Theme.textSubtle
                font.pixelSize: Theme.fontSizeCaption
            }
        }
    }
}
