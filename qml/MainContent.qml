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


    }

    Item {
        id: container;
        anchors.top: linkBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: mainSideBar.right
        anchors.right: parent.right

        property int currentPosition: height * mainSideBar.pageShownReal

        PageView {
            id: pageView
            anchors.fill: parent
            anchors.margins: 3
            pageSelectionModel: rootData.pageSelectionModel
        }
    }

    Component {
        id: renderingComponent
        RenderingView {
            width:  parent.width
            height: parent.height
            x: 0; y: -container.currentPosition
            scene: regionSceneManager.scene
        }
    }

    Component {
        id: dataMainPageComponent
        DataMainPage {
            width:  parent.width
            height: parent.height
            x: 0;
            y: height - container.currentPosition
        }
    }

    Component {
        id: unknownPageComponent
        UnknownPage {
            anchors.fill: parent
        }
    }

    Component {
        id: testcasesPageComponent
        TestcasePage {
            anchors.fill: parent
        }
    }

    Component.onCompleted: {
        pageView.unknownPageComponent = unknownPageComponent
        var viewPage = rootData.pageSelectionModel.registerPage(null, "View", renderingComponent);
        rootData.pageSelectionModel.registerPage(null, "Data", dataMainPageComponent);
        rootData.pageSelectionModel.registerPage(null, "Testcases", testcasesPageComponent);
        rootData.pageSelectionModel.gotoPage(viewPage);
    }
}
