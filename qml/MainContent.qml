import QtQuick 2.0
import QtGraphicalEffects 1.0
import QtQuick.Controls 1.0
import Cavewhere 1.0

Item {
    id: mainContentId

//    property alias dataPage: dataMainPageId

    /**
      This is for global page selection
      obj.page = "View" <- This will change the main window to "View" page
      obj.page = "Data" <- This will change the main window to "Data" page
      */
    function setCurrentMainPage(obj) {
        switch(obj.page) {
        case "View":
            mainSideBar.pageShown = 0;
            break
        case "Data":
            mainSideBar.pageShown = 1;
            break;
        default:
            console.log("Can't change the main page!!!, this is a bug");
        }
    }

    anchors.fill: parent

    LinkBar {
        id: linkBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        z: 1
    }

    MainSideBar {
        id: mainSideBar;
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -1
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: linkBar.bottom
        anchors.topMargin: -1

        //For animating which page is shown
        property real pageShownReal: pageShown;

        Behavior on pageShownReal {
            NumberAnimation {
                duration: 150
            }
        }

        //This is for global page selection
        onPageShownChanged: {
            var page;
            switch(pageShown) {
            case 0:
                page = "View";
                break;
            case 1:
                page = "Data";
                break;
            default:
                console.log("Don't know how to show page:" + pageShown);
            }

            rootData.pageSelectionModel.gotoPageByName(null, page);
        }
    }

    Item {
        id: container;
        anchors.top: linkBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: mainSideBar.right
        anchors.right: parent.right

        property int currentPosition: height * mainSideBar.pageShownReal

        PageView {
            anchors.fill: parent
            pageSelectionModel: rootData.pageSelectionModel
        }

        //       Replace with the view




//        Rectangle {
//            //visible: mainSideBar.pageShown == "draft"
//            color: "blue"
//            width:  parent.width
//            height: parent.height
//            x: 0;
//            y: height * 2 - container.currentPosition
//        }
    }

    Component {
        id: renderingComponent
        RenderingView {
//            id: terrainRendererId
            width:  parent.width
            height: parent.height
            x: 0; y: -container.currentPosition
            scene: regionSceneManager.scene
        }
    }

    Component {
        id: dataMainPageComponent
        DataMainPage {
//            id: dataMainPageId
            width:  parent.width
            height: parent.height
            x: 0;
            y: height - container.currentPosition
        }
    }

    Component.onCompleted: {
        var viewPage = rootData.pageSelectionModel.registerPage(null, "View", renderingComponent);
        rootData.pageSelectionModel.registerPage(null, "Data", dataMainPageComponent);
        rootData.pageSelectionModel.gotoPage(viewPage);

        //        var oldCurrentPage = rootData.pageSelectionModel.currentPage; //Set the current root so reloading qml works
//        rootData.pageSelectionModel.registerRootPage(mainContentId);
//        rootData.pageSelectionModel.registerPageLink(mainContentId, terrainRendererId, "View", "setCurrentMainPage", {page:'View'});
//        rootData.pageSelectionModel.registerPageLink(mainContentId, dataMainPageId, "Data", "setCurrentMainPage", {page:'Data'});
//        if(rootData.pageSelectionModel.currentPage === "") {
//            //We are starting up for the first time
//            rootData.pageSelectionModel.setCurrentPage(mainContentId, "View");
//        } else {
//            //QML reloaded for debugging
//            rootData.pageSelectionModel.setCurrentPage(mainContentId, "View");
//            rootData.pageSelectionModel.currentPage = oldCurrentPage;
//        }
    }
}
