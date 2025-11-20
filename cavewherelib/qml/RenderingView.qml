import QtQuick as QQ
import QtQuick.Controls
import QtQuick.Layouts
import cavewherelib

SplitView {
    objectName: "viewPage"

    property alias scene: rendererId.scene
    property alias turnTableInteraction: rendererId.turnTableInteraction
    property alias leadView: rendererId.leadView
    property alias renderer: rendererId

    QQ.Item {
        SplitView.preferredWidth: parent.width - 320

        GLTerrainRenderer {
            id: rendererId
            objectName: "renderer"
            anchors.fill: parent
        }
    }

    QQ.Item {
        width: 320

        ColumnLayout {
            anchors.fill: parent

            TabBar {
                id: tabBarId
                Layout.fillWidth: true

                TabButton { text: qsTr("View") }
                TabButton { text: qsTr("Layers") }
                TabButton { text: qsTr("Export") }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: tabBarId.currentIndex

                CameraOptionsTab {
                    viewer: rendererId
                }

                KeywordTab { }
            }
        }
    }
}
