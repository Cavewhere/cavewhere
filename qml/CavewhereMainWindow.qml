import QtQuick 2.0
//import Cavewhere 1.0

Rectangle {
    id: rootQMLItem
    width: 1500
    height: 1000
//    anchors.fill: parent;

    MainSideBar {
        id: mainSideBar;
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -1
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: parent.top
        anchors.topMargin: -1


        //For animating which page is shown
        property real pageShownReal: pageShown;

        Behavior on pageShownReal {
            NumberAnimation {
                duration: 150
            }
        }
    }

    Item {
        id: container;
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: mainSideBar.right
        anchors.right: parent.right

        property int currentPosition: height * mainSideBar.pageShownReal

////       Replace with the view
//        GLTerrainRenderer {
////            visible: mainSideBar.pageShown == "view"
//            glWidget: mainGLWidget
//            cavingRegion: region
//            width:  parent.width
//            height: parent.height
//            x: 0; y: -container.currentPosition

//            Component.onCompleted: {
//                //Setup the linePlotManager with the glLinePlot
//                linePlotManager.setGLLinePlot(linePlot);
//                scrapManager.setGLScraps(scraps);
//            }
//        }

//        DataMainPage {
        Rectangle {
        //visible: mainSideBar.pageShown == "data"
            width:  parent.width
            height: parent.height
            x: 0;
            y: height - container.currentPosition
        }

        Rectangle {
            //visible: mainSideBar.pageShown == "draft"
            color: "blue"
            width:  parent.width
            height: parent.height
            x: 0;
            y: height * 2 - container.currentPosition
        }
    }

//    //There's only one shadow input text editor for the cavewhere program
//    //This make the input creation much faster for any thing that needs an editor
//    //Only one editor can be open at a time
//    GlobalShadowTextInput {
//        id: globalShadowTextInput
//    }


}
