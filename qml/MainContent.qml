import QtQuick 2.0 as QQ
import QtQuick.Controls 1.0
import Cavewhere 1.0

QQ.Item {
    id: mainContentId

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

        QQ.Behavior on pageShownReal {
            QQ.NumberAnimation {
                duration: 150
            }
        }


    }

    QQ.Item {
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

    QQ.Component {
        id: renderingComponent
        RenderingView {
            width:  parent.width
            height: parent.height
            x: 0; y: -container.currentPosition
            scene: regionSceneManager.scene
        }
    }

    QQ.Component {
        id: dataMainPageComponent
        DataMainPage {
            width:  parent.width
            height: parent.height
            x: 0;
            y: height - container.currentPosition
        }
    }

    QQ.Component {
        id: unknownPageComponent
        UnknownPage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: testcasesPageComponent
        TestcasePage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: aboutPageComponent
        AboutPage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: settingsPageComponent
        SettingsPage {
            anchors.fill: parent
        }
    }

    QQ.Component.onCompleted: {
        pageView.unknownPageComponent = unknownPageComponent
        var viewPage = rootData.pageSelectionModel.registerPage(null, "View", renderingComponent);
        rootData.pageSelectionModel.registerPage(null, "Data", dataMainPageComponent);
        rootData.pageSelectionModel.registerPage(null, "Testcases", testcasesPageComponent);
        rootData.pageSelectionModel.registerPage(null, "About", aboutPageComponent)
        rootData.pageSelectionModel.registerPage(null, "Settings", settingsPageComponent)
        rootData.pageSelectionModel.gotoPage(viewPage);
    }
}
