import QtQuick 2.0
import QtGraphicalEffects 1.0
import QtQuick.Controls 1.0

Item {

    property alias dataPage: dataMainPageId

    anchors.fill: parent

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

//        Item {
//            //            color: "#86CBFF"
//            anchors.fill: terrainRendererId

//            RadialGradient {
//                anchors.fill: parent
//                verticalOffset: parent.height * .6
//                verticalRadius: parent.height * 2
//                horizontalRadius: parent.width * 2.5
//                gradient: Gradient {
//                    GradientStop { position: 0.0; color: "#F3F8FB" } //"#3986C1" }
//                    GradientStop { position: 0.5; color: "#92D7F8" }
//                }
//            }
//        }

        //       Replace with the view
        RenderingView {
            id: terrainRendererId
            width:  parent.width
            height: parent.height
            x: 0; y: -container.currentPosition
            scene: regionSceneManager.scene
        }

        DataMainPage {
            id: dataMainPageId
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



}
