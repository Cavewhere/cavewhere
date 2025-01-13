import QtQuick as QQ
import QtQuick.Controls
import cavewherelib

SplitView {
    objectName: "viewPage"

    property alias scene: rendererId.scene
    property alias turnTableInteraction: rendererId.turnTableInteraction
    property alias leadView: rendererId.leadView

    anchors.fill: parent

    QQ.Item {
        SplitView.preferredWidth: parent.width - cameraOptionsId.implicitWidth

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
