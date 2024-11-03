import QtQuick as QQ
import QtQuick.Controls
import cavewherelib

SplitView {
    property alias scene: rendererId.scene
    property alias turnTableInteraction: rendererId.turnTableInteraction
    property alias leadView: rendererId.leadView

    anchors.fill: parent


    QQ.Item {
        SplitView.preferredWidth: parent.width * .75

        GLTerrainRenderer {
            id: rendererId
            anchors.fill: parent
        }
    }

    RenderingSideBar {
        viewer: rendererId
    }
}
