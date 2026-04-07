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

QQ.Item {
    anchors.fill: parent

    TaskFutureCombineModel {
        id: taskModelId
        models: [RootData.taskManagerModel, RootData.futureManagerModel]
    }

    QQ.Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    ColumnLayout {
        id: columnId
        anchors.centerIn: parent

        QC.Label {
            text: "Finishing these jobs"
            font.pixelSize: Theme.fontSizeXLarge
            Layout.alignment: Qt.AlignHCenter
        }

        QC.Button {
            text: "Quit Now"
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                Qt.quit();
            }
        }
    }

    TaskListView {
        width: Math.min(parent.width - 10, Math.max(parent.width * 0.2, 200))
        anchors {
            top: columnId.bottom
            bottom: parent.bottom
            horizontalCenter: parent.horizontalCenter
            margins: 10
        }
        verticalLayoutDirection: QQ.ListView.TopToBottom
    }
}
