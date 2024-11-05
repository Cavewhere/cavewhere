pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls
import QtQuick.Layouts
import cavewherelib

// TabView {
//     id: tabViewId

//     property GLTerrainRenderer viewer
//     property Tab viewTab

//     QQ.Component {
//         id: cameraOptionsTabComponentId
//         CameraOptionsTab {

//         }
//     }

//     QQ.Component {
//         id: exportTabComponentId
//         ExportViewTab {
//             view: viewer
//         }
//     }

//     function resizeCurrentTab() {
//         resizing = true
//         tabViewId.Layout.maximumWidth = getTab(currentIndex).implicitWidth
//         tabViewId.Layout.minimumWidth = getTab(currentIndex).implicitWidth
//         tabViewId.Layout.maximumWidth = Number.MAX_VALUE
//         resizing = false
//     }

//     onCurrentIndexChanged: {
//         resizeCurrentTab()
//     }

//     QQ.Connections {
//         target: {
//             var tab = getTab(currentIndex)
//             if(!tab) {
//                 return null
//             }
//             return getTab(currentIndex).item
//         }
//         ignoreUnknownSignals: true
//         onImplicitWidthChanged: {
//             resizeCurrentTab()
//         }
//     }

//     QQ.Component.onCompleted: {
//         addTab("View", cameraOptionsTabComponentId);
//         addTab("Export", exportTabComponentId);

//         viewTab = getTab(0)
//     }

//     QQ.Connections {
//         target: viewTab
//         onLoaded: {
//             viewTab.item.viewer = viewer
//         }
//     }

// //    onCurrentIndexChanged: {
// //        getTab(currentIndex).item.viewer = viewer;
// //    }
// }

// import QtQuick as QQ
// import QtQuick.Controls
// import QtQuick.Layouts
// import cavewherelib

ColumnLayout {
    id: rootLayout

    required property GLTerrainRenderer viewer
    // property Tab viewTab

    TabBar {
        id: tabBarId
        Layout.fillWidth: true

        TabButton {
            text: "View"
        }
        TabButton {
            text: "Export"
        }
    }

    StackLayout {
        id: stackLayoutId
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: tabBarId.currentIndex

        // QQ.Rectangle {
        //     color: "red"
        //     anchors.fill: parent

        // }

        CameraOptionsTab {
            viewer: rootLayout.viewer
        }

        ExportViewTab {
            view: rootLayout.viewer
        }



        // QQ.Component {
        //     id: cameraOptionsTabComponentId
        //     CameraOptionsTab {
        //         QQ.Rectangle {
        //            color: "red"
        //             anchors.fill: parent
        //         }
        //     }
        // }

        // QQ.Component {
        //     id: exportTabComponentId
        //     ExportViewTab {
        //         view: rootLayout.viewer
        //     }
        // }

        // QQ.Item {
        //     QQ.Loader {
        //         sourceComponent: cameraOptionsTabComponentId
        //         onLoaded:  {
        //             (item as CameraOptionsTab).viewer = rootLayout.viewer
        //         }
        //     }
        // }

        // QQ.Item {
        //     QQ.Loader {
        //         sourceComponent: exportTabComponentId
        //         onLoaded: {
        //             (item as ExportViewTab).view = rootLayout.viewer
        //         }
        //     }
        // }
    }

    // function resizeCurrentTab() {
    //     resizing = true
    //     var currentTab = stackLayoutId.currentItem;
    //     if (currentTab) {
    //         tabBarId.Layout.maximumWidth = currentTab.implicitWidth
    //         tabBarId.Layout.minimumWidth = currentTab.implicitWidth
    //     }
    //     tabBarId.Layout.maximumWidth = Number.MAX_VALUE
    //     resizing = false
    // }

    // QQ.Connections {
    //     target: stackLayoutId.currentItem
    //     ignoreUnknownSignals: true
    //     onImplicitWidthChanged: {
    //         resizeCurrentTab()
    //     }
    // }

    QQ.Component.onCompleted: {
        tabBarId.currentIndex = 0; // Default to first tab
    }

    // QQ.Connections {
    //     target: viewTab
    //     onLoaded: {
    //         viewTab.viewer = viewer
    //     }
    // }
}

