import QtQuick as QQ
import QtQuick.Controls as QC

QC.BusyIndicator {
    id: indicator

    required property QQ.Image image

    anchors.centerIn: parent
    visible: image.visible && image.status === QQ.Image.Loading
    running: visible
}
