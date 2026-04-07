import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.Rectangle {
    id: root

    visible: false
    height: visible ? contentLayout.implicitHeight + 16 : 0
    color: Theme.warning
    radius: 5
    width: contentLayout.implicitWidth + 24

    QQ.Connections {
        target: RootData.project
        function onLfsFilesNeedSync() {
            root.visible = true;
        }
        function onSyncFinished() {
            root.visible = false;
        }
    }

    RowLayout {
        id: contentLayout
        anchors.centerIn: parent
        spacing: 12

        QC.Label {
            text: "Some files haven't been downloaded yet."
            font.pixelSize: Theme.fontSizeBody
        }

        QC.Button {
            text: "Sync"
            onClicked: RootData.project.sync()
        }

        QC.Button {
            text: "Later"
            onClicked: root.visible = false
        }
    }
}
