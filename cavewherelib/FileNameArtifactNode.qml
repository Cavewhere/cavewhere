import QtQuick
import QuickQanava as Qan
import cavewherelib

Qan.NodeItem {

    property FileNameArtifact artifact

    implicitWidth: 50
    implicitHeight: 25

    Rectangle {
        anchors.fill: parent;
        border.width: 1

        Text {
            id: textId
            anchors.centerIn: parent
            text: artifact.filename
        }
    }
}
