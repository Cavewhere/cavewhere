import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QC.SplitView {
    objectName: "viewPage"

    property alias scene: rendererId.scene
    property alias turnTableInteraction: rendererId.turnTableInteraction
    property alias leadView: rendererId.leadView
    property alias renderer: rendererId

    QQ.Item {
        QC.SplitView.preferredWidth: parent.width - cameraOptionsId.implicitWidth

        GLTerrainRenderer {
            id: rendererId
            objectName: "renderer"
            anchors.fill: parent
        }
    }

    CameraOptionsTab {
        id: cameraOptionsId
        viewer: rendererId
    }
}
