import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Cavewhere 1.0

Item {
    id: containerId
    property Renderer3D viewer
    property alias turnTableInteraction: cameraOptionsTabId.turnTableInteraction

    width: stackLayoutId.implicitWidth

    ColumnLayout {
        spacing:  0

        anchors.fill: parent

        TabBar {
            id: tabBarId

            Layout.fillWidth: true

            TabButton {
                text: "View"
            }

            TabButton {
                text: "Layers"
            }

            TabButton {
                text: "Export"
            }
        }

        StackLayout {
            id: stackLayoutId

            Layout.fillHeight: true
            Layout.fillWidth: true

            currentIndex: tabBarId.currentIndex

            CameraOptionsTab {
                id: cameraOptionsTabId
            }

            KeywordTab {
               id: keywordTabId
            }

            ExportViewTab {
                id: exportViewTabId
                view: viewer
            }
        }
    }
}
