import QtQuick
import QtQuick.Shapes
import QtQuick.Controls as QC
import QtQuick.Layouts
import CaveWhereSketch
import cavewherelib

Window {
    id: windowId
    width: 640
    height: 480
    visible: true
    title: qsTr("Hello World")

    function fuzzyEquals(value1, value2) {
        return Math.abs(value1 - value2) <= 0.0001;
    }

    RowLayout {
        id: toolBar

        QC.ToolButton {
            text: "Data"
            onClicked: {
                RootData.pageSelectionModel.currentPageAddress = "Trip"
            }
        }

        QC.ToolButton {
            text: "Sketch"
            onClicked: {
                RootData.pageSelectionModel.currentPageAddress = "Sketch"
            }
        }
    }

    Item {
        id: container;
        anchors.top: toolBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        // property int currentPosition: height * mainSideBar.pageShownReal

        PageView {
            id: pageView
            anchors.fill: parent
            pageSelectionModel: RootData.pageSelectionModel

            Component.onCompleted: {
                RootData.pageView = pageView
            }
        }
    }

    Item {
        id: overlay
        anchors.fill: parent
    }

    Component {
        id: unknownPageComponent
        UnknownPage {
            anchors.fill: parent
        }
    }

    Component {
        id: sketchPageComponent
        SketchPage {
            anchors.fill: parent
        }
    }

    Component {
        id: tripPageComponent
        TripCompactPage {
            anchors.fill: parent
        }
    }

    Component.onCompleted: {
        GlobalShadowTextInput.parent = overlay;
        RootPopupItem.parent = overlay

        pageView.unknownPageComponent = unknownPageComponent
        let tripPage = RootData.pageSelectionModel.registerPage(null, "Trip", tripPageComponent);
        let sketchPage = RootData.pageSelectionModel.registerPage(null, "Sketch", sketchPageComponent);
        // let mapPage = RootData.pageSelectionModel.registerPage(null, "Map", mapPageComponent)
        // RootData.pageSelectionModel.registerPage(null, "Testcases", testcasesPageComponent);
        // RootData.pageSelectionModel.registerPage(null, "About", aboutPageComponent)
        // RootData.pageSelectionModel.registerPage(null, "Settings", settingsPageComponent)
        // RootData.pageSelectionModel.registerPage(null, "Pipeline", pipelinePageComponent)
        RootData.pageSelectionModel.gotoPage(sketchPage);

        // mainContentId.renderer = pageView.pageItem(viewPage).renderer;
    }

}
