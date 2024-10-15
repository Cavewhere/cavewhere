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

        QQ.Rectangle {
            id: backgroundId
            anchors.fill: parent
            gradient: QQ.Gradient {
                QQ.GradientStop { position: 0.0; color: "#92D7F8" }
                QQ.GradientStop { position: 0.95; color: "#F3F8FB" }
            }
        }

        GLTerrainRenderer {
            id: rendererId
            anchors.fill: parent
        }
    }

    RenderingSideBar {
        viewer: rendererId
    }
}
