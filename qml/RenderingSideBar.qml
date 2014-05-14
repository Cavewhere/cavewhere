import QtQuick 2.0
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1
import Cavewhere 1.0

TabView {
    id: tabViewId

    Layout.minimumWidth: 175

    property RegionViewer viewer
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

    Component.onCompleted: {
        addTab("View", cameraOptionsTabComponentId);
        addTab("Export", exportTabComponentId);

        viewTab = getTab(0)

//        console.log(getTab(0).item + " " + viewer)

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
