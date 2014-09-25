import QtQuick 2.0
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1
import Cavewhere 1.0

TabView {
    id: tabViewId

//    Layout.minimumWidth: 175

    property GLTerrainRenderer viewer
    property Tab viewTab

    Component {
        id: cameraOptionsTabComponentId
        CameraOptionsTab {

        }
    }

    Component {
        id: exportTabComponentId
        ExportViewTab {
            view: viewer
        }
    }

    function resizeCurrentTab() {
        tabViewId.Layout.maximumWidth = getTab(currentIndex).implicitWidth
        tabViewId.Layout.minimumWidth = getTab(currentIndex).implicitWidth
        tabViewId.Layout.maximumWidth = Number.MAX_VALUE
    }

    onCurrentIndexChanged: {
        resizeCurrentTab()
    }

    Connections {
        target: {
            var tab = getTab(currentIndex)
            if(!tab) {
                return null
            }
            return getTab(currentIndex)
        }
        ignoreUnknownSignals: true
        onImplicitWidthChanged: {
//            resizeCurrentTab()
        }
    }

    Component.onCompleted: {
        addTab("View", cameraOptionsTabComponentId);
        addTab("Export", exportTabComponentId);

        viewTab = getTab(0)
    }

    Connections {
        target: viewTab
        onLoaded: {
            viewTab.item.viewer = viewer
        }
    }

//    onCurrentIndexChanged: {
//        getTab(currentIndex).item.viewer = viewer;
//    }
}
