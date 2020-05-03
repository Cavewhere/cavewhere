import QtQuick 2.0 as QQ
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0


SplitView {
    property alias turnTableInteraction: renderId.turnTableInteraction
    property alias leadView: renderId.leadView

    anchors.fill: parent


    QQ.Item {
        Layout.fillHeight: true
        Layout.fillWidth: true

        Layout.preferredWidth: parent.width * .75

        QQ.Item {
            anchors.fill: renderId

            RadialGradient {
                anchors.fill: parent
                verticalOffset: parent.height * .6
                verticalRadius: parent.height * 2
                horizontalRadius: parent.width * 2.5
                gradient: QQ.Gradient {
                    QQ.GradientStop { position: 0.0; color: "#F3F8FB" } //"#3986C1" }
                    QQ.GradientStop { position: 0.5; color: "#92D7F8" }
                }
            }
        }

        Renderer3D {
            id: renderId
            anchors.fill: parent
        }
    }

    RenderingSideBar {
        turnTableInteraction: renderId.turnTableInteraction
        viewer: renderId
    }
}
