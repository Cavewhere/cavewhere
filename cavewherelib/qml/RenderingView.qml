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
            objectName: "renderingSidePanel"

            TabBar {
                id: tabBarId
                objectName: "renderingTabBar"
                Layout.fillWidth: true

                TabButton { objectName: "viewTabButton"; text: qsTr("View") }
                TabButton { objectName: "layersTabButton"; text: qsTr("Layers") }
                TabButton { objectName: "exportTabButton"; text: qsTr("Export") }
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
