import QtQuick as QQ
import QtQuick.Controls
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    id: rootId
    objectName: "viewPage"

    readonly property int sidePanelWidth: 320
    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse

    property alias scene: rendererId.scene
    property alias turnTableInteraction: rendererId.turnTableInteraction
    property alias leadView: rendererId.leadView
    property alias renderer: rendererId
    property alias viewDrawer: drawerId

    SplitView {
        id: splitViewId
        anchors.fill: parent

        QQ.Item {
            SplitView.preferredWidth: parent.width - rootId.sidePanelWidth

            GLTerrainRenderer {
                id: rendererId
                objectName: "renderer"
                anchors.fill: parent
            }
        }

        QQ.Item {
            visible: !rootId.isNarrow
            width: rootId.sidePanelWidth

            LayoutItemProxy { target: sidePanelContent; anchors.fill: parent }
        }
    }

    ColumnLayout {
        id: sidePanelContent
        objectName: "renderingSidePanel"

        TabBar {
            id: tabBarId
            objectName: "renderingTabBar"
            Layout.fillWidth: true

            TabButton { objectName: "viewTabButton"; text: qsTr("View") }
            TabButton { objectName: "layersTabButton"; text: qsTr("Layers") }
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

    QQ.Column {
        id: floatingButtonsId
        objectName: "floatingButtons"
        parent: rendererId
        visible: rootId.isNarrow
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        z: 1
        spacing: 4

        RoundButton {
            objectName: "cameraButton"
            icon.source: "qrc:/twbs-icons/icons/camera-video.svg"
            icon.color: Theme.text
            onClicked: { tabBarId.currentIndex = 0; drawerId.open() }
        }
        RoundButton {
            objectName: "layersButton"
            icon.source: "qrc:/twbs-icons/icons/layers.svg"
            icon.color: Theme.text
            onClicked: { tabBarId.currentIndex = 1; drawerId.open() }
        }
    }

    Drawer {
        id: drawerId
        objectName: "viewDrawer"
        edge: Qt.RightEdge
        width: Math.min(rootId.sidePanelWidth, rootId.width * 0.85)
        // Drawer is a Popup, not sized by its declaring parent — use window height
        height: rootId.QQ.Window.window ? rootId.QQ.Window.window.height : rootId.height
        interactive: rootId.isNarrow
        dragMargin: 12

        LayoutItemProxy { target: sidePanelContent; anchors.fill: parent }
    }

    onIsNarrowChanged: if (!isNarrow) drawerId.close()
}
