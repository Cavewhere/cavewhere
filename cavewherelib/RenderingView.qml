import QtQuick 2.0 as QQ
import QtQuick.Controls
import QtQuick.Layouts 1.1
import cavewherelib
// import QtGraphicalEffects 1.0


SplitView {
    property alias scene: rendererId.scene
    property alias turnTableInteraction: rendererId.turnTableInteraction
    property alias leadView: rendererId.leadView

    anchors.fill: parent


    QQ.Item {
        Layout.fillHeight: true
        Layout.fillWidth: true

        Layout.preferredWidth: parent.width * .75

        // QQ.Item {
        //     anchors.fill: rendererId

        //     RadialGradient {
        //         anchors.fill: parent
        //         verticalOffset: parent.height * .6
        //         verticalRadius: parent.height * 2
        //         horizontalRadius: parent.width * 2.5
        //         gradient: QQ.Gradient {
        //             QQ.GradientStop { position: 0.0; color: "#F3F8FB" } //"#3986C1" }
        //             QQ.GradientStop { position: 0.5; color: "#92D7F8" }
        //         }
        //     }
        // }

        GLTerrainRenderer {
            id: rendererId
            anchors.fill: parent
        }
    }

    RenderingSideBar {
        viewer: rendererId
    }
}
