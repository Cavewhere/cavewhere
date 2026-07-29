import QtQuick as QQ
import QtQuick.Controls
import QtQuick.Layouts
import cavewherelib
import "ToolModelUtils.js" as ToolModelUtils

// The View page contributes the 3D tools to the sidebar tool rail via the
// typed ToolProviderPage contract (tools + interactionManager), sourced from
// the renderer below.
ToolProviderPage {
    id: rootId
    objectName: "viewPage"

    readonly property int sidePanelWidth: 320
    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse

    // Where the Tools tab sits in the bar, named because two places have to
    // agree on it: the button that selects it and the reset that leaves it.
    readonly property int toolsTabIndex: 2

    // Whether the sidebar's tool rail is off screen, which is the only width
    // where this page should offer tools of its own. It is a different question
    // from isNarrow — that one asks whether the side panel still fits beside the
    // renderer, and it is measured on the page, which has already had the
    // sidebar subtracted from it. Asking it of the page too would leave a band
    // (window 500 to 650) where the rail and these controls are both up, arming
    // the same tools twice.
    readonly property bool toolRailShown: rootId.QQ.Window.window
                                          ? rootId.QQ.Window.window.width >= Theme.breakpointMedium
                                          : true

    // Which tool the floating Tools button should be wearing. With the rail off
    // screen this button is the only thing left that can say what is armed.
    readonly property ToolItem armedTool: ToolModelUtils.activeTool(
                                              rootId.interactionManager, rootId.tools)

    property alias scene: rendererId.scene
    property alias turnTableInteraction: rendererId.turnTableInteraction
    property alias leadView: rendererId.leadView
    property alias renderer: rendererId
    property alias viewDrawer: drawerId

    tools: rendererId.tools
    interactionManager: rendererId.interactionManager

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

            // Only where the sidebar's tool rail is not on screen. At wider
            // widths the rail and its flyout already own the tools, and a second
            // copy in the side panel would be two places to arm the same thing.
            TabButton {
                objectName: "toolsTabButton"
                visible: !rootId.toolRailShown
                width: visible ? implicitWidth : 0
                text: qsTr("Tools")
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBarId.currentIndex

            CameraOptionsTab {
                viewer: rendererId
            }

            KeywordTab { }

            // Loaded rather than merely hidden: a StackLayout builds every child
            // whatever the current index, and the tab's options card would then
            // be a second live instance of the armed tool's property content,
            // answering to the same objectNames as the rail's flyout copy.
            QQ.Loader {
                active: !rootId.toolRailShown

                sourceComponent: ToolsTab {
                    objectName: "toolsTab"
                    interactionManager: rootId.interactionManager
                    toolModel: rootId.tools
                }
            }
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
        // Wears the armed tool's own icon, so the column reports what is armed
        // as well as opening the list — the rail that used to do that is not on
        // screen at these widths.
        RoundButton {
            objectName: "toolsButton"
            visible: !rootId.toolRailShown
            icon.source: rootId.armedTool
                         ? rootId.armedTool.iconSource
                         : "qrc:/twbs-icons/icons/tools.svg"
            icon.color: rootId.armedTool ? Theme.accent : Theme.text
            onClicked: { tabBarId.currentIndex = rootId.toolsTabIndex; drawerId.open() }
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

    // The Tools tab button goes away with the rail, but a TabBar leaves
    // currentIndex where it was — so the side panel would come back up showing
    // the tools it is not supposed to carry at these widths, with no tab in the
    // bar looking selected.
    onToolRailShownChanged: {
        if (rootId.toolRailShown && tabBarId.currentIndex === rootId.toolsTabIndex) {
            tabBarId.currentIndex = 0
        }
    }
}
