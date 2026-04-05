import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.Rectangle {
    id: root

    property bool dismissed: false

    visible: RootData.project.saveWillCauseDataLoss && !dismissed
    height: visible ? contentLayout.implicitHeight + 12 : 0
    width: visible ? contentLayout.implicitWidth + 16 : 0
    color: Theme.warning
    radius: 5
    opacity: 0.9

    QQ.Connections {
        target: RootData.project
        function onLoaded() {
            root.dismissed = false
        }
    }

    RowLayout {
        id: contentLayout
        anchors.centerIn: parent
        spacing: 8

        ColumnLayout {
            spacing: 2

            QC.Label {
                text: "Read-only"
                font.bold: true
                font.pixelSize: Theme.fontSizeBody
                Layout.alignment: Qt.AlignHCenter
            }

            QC.Label {
                text: "Upgrade to CaveWhere v" + RootData.project.requiredVersion + " to edit"
                font.pixelSize: Theme.fontSizeCaption
                Layout.alignment: Qt.AlignHCenter
            }
        }

        QC.Button {
            text: "Dismiss"
            onClicked: root.dismissed = true
        }
    }
}
