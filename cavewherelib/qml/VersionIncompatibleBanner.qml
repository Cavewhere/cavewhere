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

            QQ.Text {
                text: "Read-only"
                font.bold: true
                font.pixelSize: 13
                color: Theme.text
                Layout.alignment: Qt.AlignHCenter
            }

            QQ.Text {
                text: "Upgrade to CaveWhere v" + RootData.project.requiredVersion + " to edit"
                font.pixelSize: 11
                color: Theme.text
                Layout.alignment: Qt.AlignHCenter
            }
        }

        QC.Button {
            text: "Dismiss"
            onClicked: root.dismissed = true
        }
    }
}
