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

    FutureFilterModel {
        id: futureFilterId
        sourceModel: taskModelCombinerId
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

        height: columnLayoutId.height + 10

        color: Theme.background

        ColumnLayout {
            id: columnLayoutId

            y: 5

            anchors.margins: 5
            anchors.left: parent.left
            anchors.right: parent.right

            Text {
                id: nameText
                text: delegateId.nameRole
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            QC.ProgressBar {
                Layout.fillWidth: true
                value: !indeterminate ? delegateId.progressRole / delegateId.numberOfStepsRole : 0.0
                indeterminate: delegateId.numberOfStepsRole <= 0
            }
        }
    }
}
