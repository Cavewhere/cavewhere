import QtQuick 2.0 as QQ
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1
import Cavewhere 1.0

TabView {
    id: tabViewId

    property GLTerrainRenderer viewer
    property Tab viewTab

    QQ.Component {
        id: cameraOptionsTabComponentId
        CameraOptionsTab {

        }
    }

    QQ.Component {
        id: exportTabComponentId
        ExportViewTab {
            view: viewer
        }
    }

    function resizeCurrentTab() {
        resizing = true
        tabViewId.Layout.maximumWidth = getTab(currentIndex).implicitWidth
        tabViewId.Layout.minimumWidth = getTab(currentIndex).implicitWidth
        tabViewId.Layout.maximumWidth = Number.MAX_VALUE
        resizing = false
    }

    onCurrentIndexChanged: {
        resizeCurrentTab()
    }

    QQ.Connections {
        target: {
            var tab = getTab(currentIndex)
            if(!tab) {
                return null
            }
            return getTab(currentIndex).item
        }
        ignoreUnknownSignals: true
        onImplicitWidthChanged: {
            resizeCurrentTab()
        }
    }

    QQ.Component.onCompleted: {
        addTab("View", cameraOptionsTabComponentId);
        addTab("Export", exportTabComponentId);

        viewTab = getTab(0)
    }

    QQ.Connections {
        target: viewTab
        onLoaded: {
            viewTab.item.viewer = viewer
        }
    }

//    onCurrentIndexChanged: {
//        getTab(currentIndex).item.viewer = viewer;
//    }
}
